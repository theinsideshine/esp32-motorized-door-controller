# simulacion_planta.py
#
# Proyecto: esp32-motorized-door-controller
# Etapa: Step 8F - Identificacion de planta y simulacion PD/PID
#
# Criterio usado:
# 1. Levantar curvas reales con PWM fijo.
# 2. Identificar modelo de planta.
# 3. Simular planta discreta.
# 4. Simular PD primero: Ki = 0.
# 5. Agregar Ki chico solo para compensar error residual cerca del setpoint.
#
# Modelo de planta identificado:
#   velocidad_deg_s = Kv * (PWM - PWM_dead)
#
# Modelo discreto:
#   travel[n] = travel[n-1] + velocidad[n] * dt
#
# Control:
#   error = target - travel
#   P = Kp * error
#   I = Ki * integral(error)
#   D = -Kd * velocidad
#
#   pwm_raw = P + I + D
#
# Mejoras incluidas en esta version:
# - PWM_dead dinamico de la planta.
# - PWM minimo efectivo opcional para evitar comandos muy bajos.
# - Anti-windup.
# - Integral activada solo cerca del setpoint.
# - Comparacion open-loop real vs simulado.
# - Barrido PD y PID.
# - Guardado automatico de graficos en carpeta simulation_output.

import os
import numpy as np
import matplotlib.pyplot as plt


# ============================================================
# Configuracion general
# ============================================================

PLOT_PERIOD_S = 0.050       # Step8F: PLANT_PLOT_PERIOD_MS = 50 ms
CONTROL_DT_S = 0.005        # Firmware: control_period_us = 5000 us
ARRIVAL_TOL_DEG = 1.5

# Limites iniciales de simulacion.
# Usamos 80 porque fue el mayor PWM de identificacion levantado en Step8F.
PWM_MAX = 80.0
PWM_MIN = 0.0

# Modelo identificado aproximado.
KV_DEFAULT = 4.97            # deg/s/PWM
PWM_DEAD_DEFAULT = 18.0      # zona muerta dinamica extrapolada

# PWM minimo practico.
# No es el PWM_dead matematico.
# Es una proteccion de control para no mandar PWM bajos que en la planta real
# pueden no mover o mover de forma poco confiable.
PWM_MIN_EFFECTIVE = 55.0

# Zona mínima para aplicar PWM mínimo efectivo.
# Si el error es menor a este valor, no se fuerza el PWM a 55,
# para evitar microempujes cerca del target.
PWM_MIN_EFFECTIVE_ERROR_DEG = 6.0

# Modo para tratar PWM bajos en el controlador:
#
# "none":
#   No modifica el pwm_raw. La planta se mueve segun el modelo dinamico.
#
# "deadband":
#   Si pwm_raw esta entre 0 y PWM_MIN_EFFECTIVE, se manda 0.
#   Representa no intentar mover con PWM bajo.
#
# "lift":
#   Si pwm_raw esta entre 0 y PWM_MIN_EFFECTIVE y aun hay error,
#   se eleva a PWM_MIN_EFFECTIVE.
#   Representa una estrategia futura de PWM minimo efectivo.
#
# Para estudiar un controlador candidato conviene "lift".
# Para ver el PID matematico puro, usar "none".
CONTROL_MIN_MODE = "lift"

# Dinamica de velocidad.
# Si TAU_VEL_S = 0, la velocidad cambia instantaneamente.
# Si TAU_VEL_S > 0, agrega una inercia simple de primer orden.
TAU_VEL_S = 0.030

# Integral:
# La integral no se usa para arrancar.
# Se habilita solo cuando el error ya es relativamente bajo.
I_ACTIVE_ERROR_DEG = 35.0

# Anti-windup:
INTEGRAL_LIMIT = 120.0

# Salida de graficos
SAVE_FIGURES = True
SHOW_FIGURES = True
OUTPUT_DIR = "simulation_output"


# ============================================================
# Datos reales Step 8F
# ============================================================

CURVES = [
    {
        "name": "PWM70_POS1_POS3",
        "pwm": 70,
        "direction": "POS_1 -> POS_3",
        "target": 156.97,
        "travel": [
            0.07, 1.36, 10.11, 20.72, 31.82, 44.38, 57.04, 69.59,
            83.32, 96.44, 109.53, 123.55, 136.93, 149.44, 155.65, 155.65,
        ],
        "observed_start_delay_s": None,
    },
    {
        "name": "PWM70_POS3_POS1",
        "pwm": 70,
        "direction": "POS_3 -> POS_1",
        "target": 159.53,
        "travel": [
            0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.02, 0.00,
            0.04, 0.02, 0.00, 0.04, 0.00, 0.00, 0.04, 0.04,
            0.04, 0.00, 0.18, 7.58, 17.82, 29.09, 41.29, 54.23,
            66.47, 80.02, 93.41, 106.39, 120.30, 133.11, 146.27,
            158.75, 158.75,
        ],
        "observed_start_delay_s": None,
    },
    {
        "name": "PWM75_POS1_POS3",
        "pwm": 75,
        "direction": "POS_1 -> POS_3",
        "target": 157.06,
        "travel": [
            0.02, 1.08, 9.84, 21.58, 33.62, 47.31, 61.30, 75.23,
            90.66, 104.26, 120.01, 134.52, 148.27, 157.06, 157.06,
        ],
        "observed_start_delay_s": None,
    },
    {
        "name": "PWM75_POS3_POS1",
        "pwm": 75,
        "direction": "POS_3 -> POS_1",
        "target": 159.53,
        "travel": [
            0.00, 1.08, 10.37, 22.24, 35.11, 48.71, 62.53, 77.10,
            92.00, 106.50, 121.77, 135.31, 150.34, 159.46, 159.46,
        ],
        "observed_start_delay_s": None,
    },
    {
        "name": "PWM80_POS1_POS3",
        "pwm": 80,
        "direction": "POS_1 -> POS_3",
        "target": 157.37,
        "travel": [
            0.11, 1.36, 11.98, 24.74, 38.50, 53.77, 68.53, 84.79,
            100.02, 115.60, 131.84, 148.07, 156.91, 156.91,
        ],
        "observed_start_delay_s": None,
    },
    {
        "name": "PWM80_POS3_POS1",
        "pwm": 80,
        "direction": "POS_3 -> POS_1",
        "target": 159.50,
        "travel": [
            0.00, 1.65, 12.13, 25.20, 38.74, 54.21, 68.75, 85.23,
            100.66, 116.56, 132.80, 147.79, 159.41, 159.41,
        ],
        "observed_start_delay_s": None,
    },
]


# ============================================================
# Utilidades
# ============================================================

def ensure_output_dir():
    if SAVE_FIGURES:
        os.makedirs(OUTPUT_DIR, exist_ok=True)


def save_current_figure(filename):
    if SAVE_FIGURES:
        path = os.path.join(OUTPUT_DIR, filename)
        plt.savefig(path, dpi=150)
        print(f"Grafico guardado: {path}")


def time_vector_for_curve(travel):
    return np.arange(len(travel), dtype=float) * PLOT_PERIOD_S


def estimate_start_delay_s(travel, threshold_deg=1.0):
    travel = np.asarray(travel, dtype=float)
    idx = np.where(travel > threshold_deg)[0]

    if len(idx) == 0:
        return None

    return float(idx[0] * PLOT_PERIOD_S)


def estimate_arrival_time_s(travel, target, tol_deg=ARRIVAL_TOL_DEG):
    travel = np.asarray(travel, dtype=float)
    idx = np.where(travel >= target - tol_deg)[0]

    if len(idx) == 0:
        return None

    return float(idx[0] * PLOT_PERIOD_S)


def estimate_useful_velocity_deg_s(travel, target):
    """
    Pendiente en zona util.

    Se usa regresion lineal entre 10% y 85% del recorrido.
    Asi se evita:
    - arranque,
    - muestras repetidas,
    - zona final de corte.
    """
    travel = np.asarray(travel, dtype=float)
    t = time_vector_for_curve(travel)

    lower = 0.10 * target
    upper = 0.85 * target

    mask = (travel >= lower) & (travel <= upper)

    if np.sum(mask) < 3:
        return None

    slope, offset = np.polyfit(t[mask], travel[mask], 1)
    return float(slope)


def analyze_curve(curve):
    travel = np.asarray(curve["travel"], dtype=float)
    target = float(curve["target"])

    velocity = estimate_useful_velocity_deg_s(travel, target)
    start_delay = estimate_start_delay_s(travel)
    arrival_time = estimate_arrival_time_s(travel, target)

    final_travel = float(travel[-1])
    final_error = target - final_travel

    return {
        "name": curve["name"],
        "pwm": curve["pwm"],
        "direction": curve["direction"],
        "target": target,
        "samples": len(travel),
        "total_time_s": len(travel) * PLOT_PERIOD_S,
        "start_delay_s": start_delay,
        "arrival_time_s": arrival_time,
        "velocity_deg_s": velocity,
        "final_travel": final_travel,
        "final_error": final_error,
    }


def fit_velocity_model(results, direction_filter=None):
    """
    Ajuste:
      velocity = Kv * (PWM - PWM_dead)

    Equivalente:
      velocity = Kv * PWM + b
      PWM_dead = -b / Kv
    """
    valid = []

    for r in results:
        if r["velocity_deg_s"] is None:
            continue

        if direction_filter is not None and r["direction"] != direction_filter:
            continue

        valid.append(r)

    if len(valid) < 2:
        raise ValueError("No hay suficientes puntos para ajustar modelo.")

    pwm = np.array([r["pwm"] for r in valid], dtype=float)
    vel = np.array([r["velocity_deg_s"] for r in valid], dtype=float)

    kv, b = np.polyfit(pwm, vel, 1)
    pwm_dead = -b / kv

    return float(kv), float(pwm_dead)


def print_analysis(results):
    print("\n=== Analisis de curvas reales Step 8F ===\n")

    header = (
        f"{'Curva':<20} {'PWM':>5} {'Sentido':<15} {'Target':>9} "
        f"{'Vel util':>10} {'Delay':>10} {'Llegada':>10} {'Err final':>10}"
    )
    print(header)
    print("-" * len(header))

    for r in results:
        vel = "--" if r["velocity_deg_s"] is None else f"{r['velocity_deg_s']:.2f}"
        delay = "--" if r["start_delay_s"] is None else f"{r['start_delay_s']:.2f}"
        arrival = "--" if r["arrival_time_s"] is None else f"{r['arrival_time_s']:.2f}"

        print(
            f"{r['name']:<20} "
            f"{r['pwm']:>5} "
            f"{r['direction']:<15} "
            f"{r['target']:>9.2f} "
            f"{vel:>10} "
            f"{delay:>10} "
            f"{arrival:>10} "
            f"{r['final_error']:>10.2f}"
        )


# ============================================================
# Modelo de planta
# ============================================================

def pwm_to_steady_velocity_deg_s(pwm, kv, pwm_dead):
    """
    Modelo dinamico en regimen.

    pwm <= pwm_dead:
      velocidad = 0

    pwm > pwm_dead:
      velocidad = Kv * (pwm - pwm_dead)
    """
    pwm = max(0.0, float(pwm))

    if pwm <= pwm_dead:
        return 0.0

    return kv * (pwm - pwm_dead)


def simulate_open_loop(
    pwm,
    target,
    kv,
    pwm_dead,
    dt_s=CONTROL_DT_S,
    t_max_s=2.5,
    start_delay_s=0.0,
    tau_vel_s=TAU_VEL_S,
):
    """
    Simula PWM fijo.

    Se usa para comparar contra las curvas reales.
    El delay de arranque puede ser tomado de los datos reales.
    """
    n = int(t_max_s / dt_s) + 1
    t = np.arange(n) * dt_s

    travel = np.zeros(n)
    velocity = np.zeros(n)
    pwm_cmd = np.zeros(n)

    stopped = False

    for i in range(1, n):
        if stopped:
            travel[i] = travel[i - 1]
            velocity[i] = 0.0
            pwm_cmd[i] = 0.0
            continue

        if t[i] < start_delay_s:
            pwm_now = pwm
            v_ss = 0.0
        else:
            pwm_now = pwm
            v_ss = pwm_to_steady_velocity_deg_s(pwm_now, kv, pwm_dead)

        if tau_vel_s > 0:
            alpha = min(dt_s / tau_vel_s, 1.0)
            velocity[i] = velocity[i - 1] + alpha * (v_ss - velocity[i - 1])
        else:
            velocity[i] = v_ss

        travel[i] = travel[i - 1] + velocity[i] * dt_s
        pwm_cmd[i] = pwm_now

        if travel[i] >= target - ARRIVAL_TOL_DEG:
            stopped = True
            travel[i] = min(travel[i], target)
            velocity[i] = 0.0
            pwm_cmd[i] = 0.0

    return {
        "t": t,
        "travel": travel,
        "velocity": velocity,
        "pwm": pwm_cmd,
        "target": target,
    }


# ============================================================
# Controlador PD/PID discreto
# ============================================================

def apply_control_pwm_rules(raw_pwm, error, mode=CONTROL_MIN_MODE):
    """
    Convierte la salida matematica del controlador en PWM aplicado.

    mode = "none":
      No se aplica regla extra.

    mode = "deadband":
      PWM bajo se anula.

    mode = "lift":
      PWM bajo se eleva a PWM_MIN_EFFECTIVE solo si el error
      todavia es suficientemente grande.
    """
    pwm = max(PWM_MIN, min(raw_pwm, PWM_MAX))

    if abs(error) <= ARRIVAL_TOL_DEG:
        return pwm

    if mode == "none":
        return pwm

    if mode == "deadband":
        if 0.0 < pwm < PWM_MIN_EFFECTIVE:
            return 0.0
        return pwm

    if mode == "lift":
        if abs(error) > PWM_MIN_EFFECTIVE_ERROR_DEG:
            if 0.0 < pwm < PWM_MIN_EFFECTIVE:
                return PWM_MIN_EFFECTIVE
        return pwm

    raise ValueError(f"CONTROL_MIN_MODE invalido: {mode}")

def simulate_pid(
    target,
    kp,
    ki,
    kd,
    kv,
    pwm_dead,
    dt_s=CONTROL_DT_S,
    t_max_s=2.5,
    pwm_max=PWM_MAX,
    tau_vel_s=TAU_VEL_S,
    integral_limit=INTEGRAL_LIMIT,
    i_active_error_deg=I_ACTIVE_ERROR_DEG,
    control_min_mode=CONTROL_MIN_MODE,
    stop_at_arrival=False,
):
    """
    Simulacion discreta del controlador.

    P:
      Empuja fuerte al inicio.
      Si Kp * error supera PWM_MAX, la salida satura.

    D:
      Resta PWM segun velocidad.
      Ayuda a frenar.

    I:
      No se usa para arrancar.
      Se activa solo cerca del setpoint.
      Se limita para evitar windup.

    stop_at_arrival:
      False:
        Permite ver sobrepaso/tendencia natural del controlador.

      True:
        Simula una FSM que corta al entrar en tolerancia.
    """
    global PWM_MAX

    old_pwm_max = PWM_MAX
    PWM_MAX = pwm_max

    try:
        n = int(t_max_s / dt_s) + 1
        t = np.arange(n) * dt_s

        travel = np.zeros(n)
        velocity = np.zeros(n)

        error = np.zeros(n)

        p_term = np.zeros(n)
        i_term = np.zeros(n)
        d_term = np.zeros(n)

        pwm_raw = np.zeros(n)
        pwm_applied = np.zeros(n)

        integral = 0.0
        stopped = False

        for i in range(1, n):
            previous_travel = travel[i - 1]
            previous_velocity = velocity[i - 1]

            error[i] = target - previous_travel

            if stop_at_arrival and abs(error[i]) <= ARRIVAL_TOL_DEG:
                stopped = True

            if stopped:
                travel[i] = previous_travel
                velocity[i] = 0.0
                pwm_raw[i] = 0.0
                pwm_applied[i] = 0.0
                p_term[i] = 0.0
                i_term[i] = i_term[i - 1]
                d_term[i] = 0.0
                continue

            # Termino proporcional.
            p_term[i] = kp * error[i]

            # Termino derivativo sobre medicion.
            # Si la puerta viaja hacia el target, velocity es positiva y D resta PWM.
            d_term[i] = -kd * previous_velocity

            # Integral solo cerca del setpoint.
            # Esto respeta el criterio: I no es para arrancar, es para error residual.
            integral_candidate = integral

            if abs(error[i]) <= i_active_error_deg:
                integral_candidate = integral + error[i] * dt_s
                integral_candidate = max(-integral_limit, min(integral_candidate, integral_limit))

            i_candidate = ki * integral_candidate
            raw_candidate = p_term[i] + i_candidate + d_term[i]

            # Anti-windup:
            # Si el candidato satura y el error sigue empujando en el mismo sentido,
            # no aceptamos mas integral.
            saturated_high = raw_candidate > pwm_max
            saturated_low = raw_candidate < PWM_MIN

            if saturated_high and error[i] > 0:
                # no integrar mas
                pass
            elif saturated_low and error[i] < 0:
                # no integrar mas
                pass
            else:
                integral = integral_candidate

            i_term[i] = ki * integral

            raw = p_term[i] + i_term[i] + d_term[i]
            raw = max(PWM_MIN, min(raw, pwm_max))
            pwm_raw[i] = raw

            pwm_applied[i] = apply_control_pwm_rules(
                raw_pwm=raw,
                error=error[i],
                mode=control_min_mode,
            )

            # Planta
            v_ss = pwm_to_steady_velocity_deg_s(pwm_applied[i], kv, pwm_dead)

            if tau_vel_s > 0:
                alpha = min(dt_s / tau_vel_s, 1.0)
                velocity[i] = previous_velocity + alpha * (v_ss - previous_velocity)
            else:
                velocity[i] = v_ss

            travel[i] = previous_travel + velocity[i] * dt_s

        error = target - travel

        return {
            "t": t,
            "travel": travel,
            "velocity": velocity,
            "error": error,
            "pwm_raw": pwm_raw,
            "pwm": pwm_applied,
            "p": p_term,
            "i": i_term,
            "d": d_term,
            "target": target,
            "kp": kp,
            "ki": ki,
            "kd": kd,
            "control_min_mode": control_min_mode,
        }

    finally:
        PWM_MAX = old_pwm_max


def summarize_response(sim):
    target = sim["target"]
    travel = sim["travel"]
    t = sim["t"]

    max_travel = float(np.max(travel))
    overshoot = max(0.0, max_travel - target)

    idx_arrival = np.where(travel >= target - ARRIVAL_TOL_DEG)[0]
    arrival_time = None if len(idx_arrival) == 0 else float(t[idx_arrival[0]])

    final_error = float(target - travel[-1])

    return {
        "kp": sim["kp"],
        "ki": sim["ki"],
        "kd": sim["kd"],
        "arrival_time_s": arrival_time,
        "overshoot_deg": overshoot,
        "final_error_deg": final_error,
        "max_pwm_raw": float(np.max(sim["pwm_raw"])),
        "max_pwm_applied": float(np.max(sim["pwm"])),
    }


# ============================================================
# Graficos
# ============================================================

def plot_real_curves(curves):
    plt.figure(figsize=(10, 5))

    for curve in curves:
        t = time_vector_for_curve(curve["travel"])
        plt.plot(t, curve["travel"], marker=".", label=curve["name"])

    plt.xlabel("Tiempo [s]")
    plt.ylabel("Travel [deg]")
    plt.title("Curvas reales Step 8F - travel")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    save_current_figure("01_curvas_reales_step8F.png")


def plot_velocity_model(results, kv, pwm_dead):
    pwm_points = np.array(
        [r["pwm"] for r in results if r["velocity_deg_s"] is not None],
        dtype=float,
    )
    vel_points = np.array(
        [r["velocity_deg_s"] for r in results if r["velocity_deg_s"] is not None],
        dtype=float,
    )

    pwm_line = np.linspace(60, 85, 100)
    vel_line = np.array([
        pwm_to_steady_velocity_deg_s(p, kv, pwm_dead)
        for p in pwm_line
    ])

    plt.figure(figsize=(10, 5))
    plt.plot(pwm_points, vel_points, "o", label="Datos reales")
    plt.plot(pwm_line, vel_line, label=f"v = {kv:.2f} * (PWM - {pwm_dead:.2f})")
    plt.xlabel("PWM")
    plt.ylabel("Velocidad util [deg/s]")
    plt.title("Modelo identificado PWM -> velocidad")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    save_current_figure("02_modelo_pwm_velocidad.png")


def plot_open_loop_comparison(curve, kv, pwm_dead):
    real_t = time_vector_for_curve(curve["travel"])
    real_travel = np.asarray(curve["travel"], dtype=float)

    delay = estimate_start_delay_s(real_travel)
    if delay is None:
        delay = 0.0

    sim = simulate_open_loop(
        pwm=curve["pwm"],
        target=curve["target"],
        kv=kv,
        pwm_dead=pwm_dead,
        start_delay_s=delay,
        t_max_s=max(2.0, real_t[-1] + 0.5),
    )

    plt.figure(figsize=(10, 5))
    plt.plot(real_t, real_travel, "o", label="Real")
    plt.plot(sim["t"], sim["travel"], label="Simulada")
    plt.axhline(curve["target"], linestyle="--", label="Target")
    plt.xlabel("Tiempo [s]")
    plt.ylabel("Travel [deg]")
    plt.title(f"Open-loop real vs simulado - {curve['name']}")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    save_current_figure(f"03_open_loop_{curve['name']}.png")


def plot_controller_response(sim, title, filename_prefix):
    plt.figure(figsize=(10, 5))
    plt.plot(sim["t"], sim["travel"], label="Travel")
    plt.axhline(sim["target"], linestyle="--", label="Target")
    plt.xlabel("Tiempo [s]")
    plt.ylabel("Travel [deg]")
    plt.title(title)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    save_current_figure(f"{filename_prefix}_travel.png")

    plt.figure(figsize=(10, 5))
    plt.plot(sim["t"], sim["pwm_raw"], label="PWM raw PID")
    plt.plot(sim["t"], sim["pwm"], label="PWM aplicado")
    plt.xlabel("Tiempo [s]")
    plt.ylabel("PWM")
    plt.title(f"{title} - PWM")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    save_current_figure(f"{filename_prefix}_pwm.png")

    plt.figure(figsize=(10, 5))
    plt.plot(sim["t"], sim["p"], label="P")
    plt.plot(sim["t"], sim["i"], label="I")
    plt.plot(sim["t"], sim["d"], label="D")
    plt.plot(sim["t"], sim["p"] + sim["i"] + sim["d"], label="P + I + D")
    plt.xlabel("Tiempo [s]")
    plt.ylabel("PWM equivalente")
    plt.title(f"{title} - Componentes")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    save_current_figure(f"{filename_prefix}_componentes.png")


def plot_sweep(sims, title, filename):
    plt.figure(figsize=(10, 5))

    for sim in sims:
        label = f"Kp={sim['kp']:.2f}, Ki={sim['ki']:.2f}, Kd={sim['kd']:.3f}"
        plt.plot(sim["t"], sim["travel"], label=label)

    target = sims[0]["target"]
    plt.axhline(target, linestyle="--", label="Target")
    plt.xlabel("Tiempo [s]")
    plt.ylabel("Travel [deg]")
    plt.title(title)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    save_current_figure(filename)


# ============================================================
# Main
# ============================================================

def main():
    ensure_output_dir()

    results = [analyze_curve(c) for c in CURVES]
    print_analysis(results)

    kv_avg, pwm_dead_avg = fit_velocity_model(results)
    kv_p1p3, pwm_dead_p1p3 = fit_velocity_model(results, direction_filter="POS_1 -> POS_3")
    kv_p3p1, pwm_dead_p3p1 = fit_velocity_model(results, direction_filter="POS_3 -> POS_1")

    print("\n=== Modelo identificado ===\n")
    print("Modelo promedio:")
    print(f"  velocidad_deg_s = {kv_avg:.3f} * (PWM - {pwm_dead_avg:.3f})")

    print("\nModelo POS_1 -> POS_3:")
    print(f"  velocidad_deg_s = {kv_p1p3:.3f} * (PWM - {pwm_dead_p1p3:.3f})")

    print("\nModelo POS_3 -> POS_1:")
    print(f"  velocidad_deg_s = {kv_p3p1:.3f} * (PWM - {pwm_dead_p3p1:.3f})")

    print("\n=== Observacion de arranque ===\n")
    print("PWM 70 mueve bien una vez que arranca.")
    print("Pero en POS_3 -> POS_1 tuvo un retardo de arranque cercano a 0.95 s.")
    print("Por eso el modelo separa:")
    print("  1) velocidad en regimen")
    print("  2) condicion practica de PWM minimo / arranque\n")

    # Graficos de planta
    plot_real_curves(CURVES)
    plot_velocity_model(results, kv_avg, pwm_dead_avg)

    for curve in CURVES:
        plot_open_loop_comparison(curve, kv_avg, pwm_dead_avg)

    # ========================================================
    # Simulacion de control
    # ========================================================

    target = 157.0

    kp_min_to_saturate = PWM_MAX / target

    print("\n=== Criterio inicial de Kp ===\n")
    print(f"Target = {target:.1f} deg")
    print(f"PWM_MAX = {PWM_MAX:.1f}")
    print(f"Kp minimo para saturar al inicio = {kp_min_to_saturate:.3f}")
    print("Se prueban Kp alrededor de ese valor.")
    print("Ki arranca en 0 para PD.")
    print("Luego se prueba Ki chico para error residual.\n")

    # --------------------------------------------------------
    # Barrido PD
    # --------------------------------------------------------

    pd_candidates = [
        (0.55, 0.030),
        (0.60, 0.040),
        (0.65, 0.050),
        (0.70, 0.060),
        (0.80, 0.080),
    ]

    pd_sims = []

    print("=== Barrido PD - Ki = 0 ===\n")
    header = (
        f"{'Kp':>6} {'Ki':>6} {'Kd':>8} {'Llegada [s]':>14} "
        f"{'Overshoot':>12} {'Err final':>12} {'PWM raw':>10} {'PWM apl':>10}"
    )
    print(header)
    print("-" * len(header))

    for kp, kd in pd_candidates:
        sim = simulate_pid(
            target=target,
            kp=kp,
            ki=0.0,
            kd=kd,
            kv=kv_avg,
            pwm_dead=pwm_dead_avg,
            pwm_max=PWM_MAX,
            control_min_mode=CONTROL_MIN_MODE,
            stop_at_arrival=False,
        )
        pd_sims.append(sim)

        summary = summarize_response(sim)
        arrival = "--" if summary["arrival_time_s"] is None else f"{summary['arrival_time_s']:.3f}"

        print(
            f"{kp:>6.2f} "
            f"{0.0:>6.2f} "
            f"{kd:>8.3f} "
            f"{arrival:>14} "
            f"{summary['overshoot_deg']:>12.3f} "
            f"{summary['final_error_deg']:>12.3f} "
            f"{summary['max_pwm_raw']:>10.2f} "
            f"{summary['max_pwm_applied']:>10.2f}"
        )

    plot_sweep(pd_sims, "Barrido PD - Ki = 0", "04_barrido_PD.png")

    # --------------------------------------------------------
    # Barrido PID con Ki chico
    # --------------------------------------------------------

    pid_candidates = [
        (0.60, 0.10, 0.040),
        (0.65, 0.15, 0.050),
        (0.70, 0.20, 0.050),
        (0.70, 0.30, 0.060),
        (0.75, 0.25, 0.060),
    ]

    pid_sims = []

    print("\n=== Barrido PID con Ki chico ===\n")
    print(header)
    print("-" * len(header))

    for kp, ki, kd in pid_candidates:
        sim = simulate_pid(
            target=target,
            kp=kp,
            ki=ki,
            kd=kd,
            kv=kv_avg,
            pwm_dead=pwm_dead_avg,
            pwm_max=PWM_MAX,
            control_min_mode=CONTROL_MIN_MODE,
            stop_at_arrival=False,
        )
        pid_sims.append(sim)

        summary = summarize_response(sim)
        arrival = "--" if summary["arrival_time_s"] is None else f"{summary['arrival_time_s']:.3f}"

        print(
            f"{kp:>6.2f} "
            f"{ki:>6.2f} "
            f"{kd:>8.3f} "
            f"{arrival:>14} "
            f"{summary['overshoot_deg']:>12.3f} "
            f"{summary['final_error_deg']:>12.3f} "
            f"{summary['max_pwm_raw']:>10.2f} "
            f"{summary['max_pwm_applied']:>10.2f}"
        )

    plot_sweep(pid_sims, "Barrido PID - Ki chico", "05_barrido_PID_Ki_chico.png")

    # --------------------------------------------------------
    # Seleccion inicial para inspeccionar
    # --------------------------------------------------------

    selected_kp = 0.70
    selected_ki = 0.20
    selected_kd = 0.05

    selected = simulate_pid(
        target=target,
        kp=selected_kp,
        ki=selected_ki,
        kd=selected_kd,
        kv=kv_avg,
        pwm_dead=pwm_dead_avg,
        pwm_max=PWM_MAX,
        control_min_mode=CONTROL_MIN_MODE,
        stop_at_arrival=False,
    )

    plot_controller_response(
        selected,
        title=f"PID inicial - Kp={selected_kp}, Ki={selected_ki}, Kd={selected_kd}",
        filename_prefix="06_pid_inicial",
    )

    # --------------------------------------------------------
    # Misma seleccion con corte por FSM
    # --------------------------------------------------------

    selected_cut = simulate_pid(
        target=target,
        kp=selected_kp,
        ki=selected_ki,
        kd=selected_kd,
        kv=kv_avg,
        pwm_dead=pwm_dead_avg,
        pwm_max=PWM_MAX,
        control_min_mode=CONTROL_MIN_MODE,
        stop_at_arrival=True,
    )

    plot_controller_response(
        selected_cut,
        title=f"PID inicial con corte por tolerancia - Kp={selected_kp}, Ki={selected_ki}, Kd={selected_kd}",
        filename_prefix="07_pid_inicial_con_corte",
    )

    print("\n=== Configuracion usada en simulacion de control ===\n")
    print(f"CONTROL_MIN_MODE = {CONTROL_MIN_MODE}")
    print(f"PWM_MIN_EFFECTIVE = {PWM_MIN_EFFECTIVE}")
    print(f"I_ACTIVE_ERROR_DEG = {I_ACTIVE_ERROR_DEG}")
    print(f"INTEGRAL_LIMIT = {INTEGRAL_LIMIT}")
    print(f"TAU_VEL_S = {TAU_VEL_S}")
    print("\nFin de simulacion.")

    if SHOW_FIGURES:
        plt.show()
    else:
        plt.close("all")


if __name__ == "__main__":
    main()