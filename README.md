## wsbench - 房卡制棋牌游戏压测工具

### 适用场景

wsbench 适用于常见房卡制棋牌游戏压测，该游戏需有以下特征：

* 使用 websocket 作为游戏通信协议

  游戏前后端使用 *非* 二进制websocket协议进行通信，另，在游戏初始化过程中，支持 GET, POST 方式
  的HTTP请求（如 pomelo 会在游戏时先进行几次 HTTP 请求）。

* 房间制

  由房主创建房间，得到房间标识，其他用户根据该标识加入房间后开始游戏。

另，wsbench 支持同时压测多个游戏（可用作游戏业务监控）。


### 原理

房卡制棋牌游戏压测 仅仅是 socket 长连接压测中的一种，由于 H5 棋牌游戏常采用JSON文本内容，基于 websocket
协议通信，故，针对不同的游戏业务，我们仅需要为其进行特定的文本化通信配置（脚本），而无需进行协议层的二次开发。

为此，wsbench 遍历每个游戏配置文件中的 users 数组（根据room.usernumber配置为每个用户分配座位顺序）,
根据配置文件中的 server 对象, 与游戏服务器进行以下通信:

1. 遍历 urls 数组, 初始化游戏连接

2. 根据 room.actions 配置，为不同座次用户进行创建、或加入房间的动作

3. epool 所有的通信fd, 等待服务器推送消息，推送过来的消息与每个用户在 callbacks 中的座次号配置匹配，
   并触发后续动作。

4. 用户收到 callbacks 配置中 over 属性为 true 的消息时，该用户本轮游戏结束。

5. 房间内所有用户都结束本轮游戏后，重复步骤 1.


### 配置

以下，以 [斗地主游戏配置](doc/sample.json) 为例进行注解：

![初始化及创建、加入房间](doc/1.png?raw=true "init")

![绑定回调](doc/2.png?raw=true "callback")

注：

* 每轮游戏中，每个用户 callback 里的动作配置不会重复触发
 （即，同一轮游戏中，若同一个用户收到2条消息都匹配到相同回调节点时，若此回调req, over动作已经触发，便不会再次被触发）


### 调试

#### 在进行新游戏的压测脚本编写及调试时，建议进行以下配置：

  1. config.json

```js
{
    max_threadnum: 1,
    trace: { tostdout: true, level: "debug"},
    sites: [
        {name: "YOUR_GAME_NAME"}
    ]
}
```

  2. YOUR_GAME_NAME.json

参照 [斗地主例子](doc/sample.json)，users 一级数组仅包含一个元素（一个线程），该元素中仅配置需要的用户（如斗
地主只需要三个人）。

这样，只会开启一个压测线程以方便调试，所有日志都会直接输出到终端以方便查看。

若，打印出来的日志不够进行分析，可将trace.level调整成 max（也可以调整成 error 以减少输出），

又若，你需要查看原始的收发包信息，请用 make DEBUG_MSG=1 重新编译 wsbench 。


#### 游戏的配置脚本编写好测试通过后:

  1. 将 config.json 中 max_thradnum 改成 100 （或者你自己设计的线程个数）

  2. 将 config.json 中 trace.tostdout 改成 false，trace.level 改成 "error"
     这样，所有的程序日志会输出到 logs/xxx.log 文件，xxx 为每个线程的顺序id。

  3. 将 YOUR_GAME_NAME.json 中 users 数组，配置成多个（数千？）用户。是的，你需要在压测环境建立数千个测试账号以用来压测
