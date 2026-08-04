events = []


def reset_events():
    events.clear()


class PhysicsProbe:
    def __init__(self):
        self.owner = None

    def _record(self, callback_name, payload):
        try:
            payload["phase"] = "mutated"
            readonly = False
        except TypeError:
            readonly = True
        snapshot = dict(payload)
        snapshot["payload_readonly"] = readonly
        events.append((callback_name, snapshot))

    def on_collision_enter(self, payload):
        self._record("on_collision_enter", payload)

    def on_collision_stay(self, payload):
        self._record("on_collision_stay", payload)

    def on_collision_exit(self, payload):
        self._record("on_collision_exit", payload)

    def on_trigger_enter(self, payload):
        self._record("on_trigger_enter", payload)

    def on_trigger_stay(self, payload):
        self._record("on_trigger_stay", payload)

    def on_trigger_exit(self, payload):
        self._record("on_trigger_exit", payload)


class ThrowingPhysicsProbe:
    def __init__(self):
        self.owner = None

    def _fail(self):
        raise RuntimeError("intentional physics callback failure")

    def on_collision_enter(self, payload):
        self._fail()

    def on_collision_stay(self, payload):
        self._fail()

    def on_collision_exit(self, payload):
        self._fail()

    def on_trigger_enter(self, payload):
        self._fail()

    def on_trigger_stay(self, payload):
        self._fail()

    def on_trigger_exit(self, payload):
        self._fail()
