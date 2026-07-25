from queue import Queue, Empty


class SerialEventQueue:
    def __init__(self):
        self._queue = Queue()

    def put(self, item: str):
        self._queue.put(item)

    def get_nowait(self) -> str:
        return self._queue.get_nowait()

    def clear(self):
        while True:
            try:
                self._queue.get_nowait()
            except Empty:
                break