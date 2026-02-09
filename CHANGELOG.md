# ChikaEngine Changelog

简单记录开发过程，非标 changelog

## 2026-02-09

实现简易的编译期反射系统,由于静态全局函数或结构体无法自动执行,于是重新渲染一个代码用于执行注册函数.
综上所述,现在的构建流程——

1. 根目录开始
2. 在 engine 目录下添加生成 Registry.cpp 的规则
3. 继续遍历子模块的构建流程,如果遇到反射定义的宏,就代码渲染出对应的反射代码并且加入构建
4. update registey 代码并且重新加入构建
5. runtime 的时候在 engine 的入口加入已经渲染好的反射初始化函数(提前声明,但是实现在 registry 渲染出的文件中)

Todo:

- [ ] 重构 engine 构建流程,分成editor、runtime等大大模块
- [ ] 把目前引擎的其他必要部分加入反射构建系统
- [ ] 实现 inspector 窗口,支持点击修改参数

## 2026-02-06

反射系统大致规划——

1. 通过宏来标记对应的反射数据,Class, Field, Function
2. 借助clang进行AST分析,然后提取元数据
3. code generation,并且把渲染出来的代码加入主程的编译
4. 在runtime的时候对反射对象进行修改(使用反射系统)

## 2026-02-02

提供 scene 对 GO 操作的接口

Todo:

- [ ] 实现渲染组件,把渲染的pipeline整合到gameplay中
- [ ] 实现动画系统
- [ ] 实现反射系统,开始准备做 editor 的编辑和inspect窗口的数据展示/修改

## 2026-02-01

因为实现物理需要依赖——Scene中对物体的收集和Layer的概念,于是稍微构建Gameplay系统
目前实现逻辑——

- Component: 在GO的AddComponent方法中创建时,自动绑定owner为GO,然后
- GameObject(GO): 在逻辑上为组件的集合,然后有一个内部的遍历组件生命周期方法,
- Scene: 收集所有的GO —— 实现创建,销毁等逻辑

那么在物体挂载rigidbody和collider组件等时候,会调用 Physics 中的创建方法,然后借助Scene来建立一个mapping,即`<GO Handle, Rigidbody Handle>`,告诉物理系统,有一个GO身上挂载着一个物理组件.最后在Tick中更新go的transofrm组件,以达到通过物理计算修改位置的方式.

Todo:

- [ ] 实现 Scene Manager
- [ ] 完善整个代理逻辑
- [ ] 重构 Framework 和 physics 层的逻辑,目前两个耦合度太高(循环依赖),希望通过调整分离

## 2026-01-27

开始着手物理层的书写
使用`JoltPhysics`作为成熟的后端方案,所以只需要编辑接口然后把对应的Collider和Rigidbody组件做出即可;
物理系统仅对外暴露Tick等简单方法,然后持有一个后端实例(和其他系统保证一致性)

Todo:

- [ ] 或许可以把System的概念再做一层抽象,然后规范化各个系统

## 2026-01-20

editor的简单实现——

- View Panel：往渲染器上添加了一个方法，可以支持往一个fbo上写数据，最后传给imgui来输出就好
- Log Panel：把 Editor Logger sink 注册到单例中，然后自己持有指针来接收数据

TODO:

- [ ] 整理代码框架——engine逻辑不清晰
- [ ] 整理文件目录以及重写 cmake

准备着手写资源读写层，对每一种资源写一个importer，统一丢给 resource system 处理，目前需要的资源

- frag
- vert

## 2026-01-10

实现 `Input System` 和 `Time System` 工厂函数，但是需发现各个模块之间需要频繁传递 window，一种很不好的感觉

实现方式——

- 提供 desc 字段用于自定义后段类型以及其他内容，其中包含后段类型的枚举类
- 工厂接受 desc 和 arg.. 以创建对应后段单例的示例指针
- 单例接受来自工厂的后端实例
- 对外只暴露系统单例的`Init`方法，允许传入 desc 等参数

## 2026-01-08

大概已经定下来一个与平台耦合的解耦模式 ——

1. 提供对外单例接口（目前感觉每次Init很麻烦，后面需要==重构==）
2. 把后端单独抽离出来
3. 提供一个工厂方法，通过指定的后端来对应生成方法

## 2026-01-07

Add

Input System 的分层实现——

- 使用单例模式提供输入接口
- 然后把后端（和硬件交互）的部分抽离出来，方便未来做多个不同后端的适配

Modified

- 把相机作为 GO, 并且维护相机朝向作为定位标注，而不是 "target"

Todo

- [ ] 提供输入系统的工厂方法
- [ ] 重新分配依赖和重构文件目录（现在各个 Block 之间的依赖开始变得复杂且混乱）

## 2026-01-01

GO -> Transform (World Position)

Scene 层收集所有可渲染对象（Mesh Material 等）然后进行渲染（丢给 Renderer 抽象层，然后使用 OpenGL 作为示例底层）
