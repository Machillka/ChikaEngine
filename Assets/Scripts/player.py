import chika_engine


class PlayerController:
    def __init__(self):
        self.owner: chika_engine.GameObject = None
        self.speed = 0.1

    def awake(self):
        print(f"[Python] Player Controller Awake! Attached to {self.owner.GetName()}")

    def update(self, delta_time: float):
        if self.owner.transform:
            pos = self.owner.transform.position
            # 修改 C++ 的对象数据
            pos.x += self.speed * delta_time
            self.owner.transform.position = pos
