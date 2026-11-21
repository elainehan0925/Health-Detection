# Eye Monitor：摄像头数据采集

本程序负责摄像头采集、眨眼/哈欠的累计 baseline 与一分钟频率、闭眼统计和休息记录。同时输出本模块的 0–1 疲劳指数 E，最终团队总分由队友整合。

## 运行与输出

需要 Windows、MSYS2 UCRT64 的 GCC/OpenCV，脚本默认 MSYS2 位于 C:\msys64。
在 VS Code 打开整个 Utex 文件夹，F5 选择 Utex: Blink Counter；也可以运行 build.ps1，再运行 run.ps1。
默认是正常后台模式：不创建摄像头窗口，不绘制画面，不输出逐帧或周期调试日志；摄像头仍在采集，JSON 仍每五秒更新。在启动终端按 Ctrl+C 正常停止，最后保存一次 JSON 并释放摄像头。
需要画面时运行 run.ps1 -Preview，或 camera_preview.exe --preview。VS Code 的 Run eye monitor 任务是后台模式；Run camera preview 任务显示调试画面。F5 默认不带 --preview，因此也是后台模式。
--camera N 可选择摄像头，--json 路径可更改输出位置；--check / --check-frames N 用于摄像头及模型自检，不写 JSON。

默认文件是 program1/blink.json。摄像头和模型初始化成功后、处理首帧前，先写一份新的空会话快照，避免沿用上次会话的读数；运行中每隔至少 5 秒覆盖一次，退出额外保存最终一份。摄像头读帧失败会提示并以失败状态退出，最终 JSON 保存失败也会返回失败状态。

| JSON 字段 | 单位与含义 |
|---|---|
| blink_rate_per_minute | 最近真实 60 秒眨眼次数 × 60 / 同窗口可靠观测秒数 |
| baseline_blinks_per_minute | 从启动或 R 重置起眨眼总次数 × 60 / 累计可靠观测秒数 |
| yawn_rate_per_minute | 最近真实 60 秒哈欠次数 × 60 / 同窗口可靠观测秒数 |
| baseline_yawns_per_minute | 从启动或 R 重置起哈欠总次数 × 60 / 累计可靠观测秒数 |
| eyes_closure_total_seconds | 本次会话累计观测到的闭眼秒数，包含普通眨眼 |
| yawn_count | 本次会话累计哈欠候选次数 |
| rest_seconds_last_20_minutes | 最近真实 1200 秒内休息区间并集的秒数 |

JSON 字段以现有 program1/blink.json 的七个 snake_case 名称为准。该文件是正式运行输出，启动时初始化，运行中每五秒更新。
四种频率都以次/分钟计，累计基准会持续更新，不再冻结最初一分钟。分母只包含连续可靠帧之间的观测时间，不含暂停、跟踪缺失或大断帧；可靠跟踪到的闭眼也属于观测时间。
累计有效观测少于 10 秒时，两种 baseline 为 null；达到 10 秒后，即使事件次数为零也输出 0。最近一分钟有效观测不足 10 秒或覆盖率低于 70% 时，两种短期频率为 null。启动不足一分钟时，短期值是已有可靠观测上的每分钟估计。
baseline 只是本次会话的累计平均，不保证代表健康或不疲劳状态。累计哈欠基准可能为 0，评分时不能直接拿它当除数。
运行 test_live_json.ps1 可只读查看正式 blink.json 与 blink_fatigue.json 的自动更新。
JSON 不包含暂停/重置标记或历史记录。队友可用文件修改时间判断是否仍更新；累计值变小代表重置，不能作负增量。文件中不保存图片或视频。

## 程序如何配合

```text
program.cpp
  ├─ eye_detector.h        图片 → 眼睛/嘴巴/视线识别结果
  ├─ eye_activity.h        连续帧 → 眨眼、闭眼、哈欠统计
  ├─ personal_baseline.h   有效观测 → 头部参考方向
  ├─ rest_policy.h         时间区间 → 滚动休息时长与提醒
  └─ metrics_json.h        七项结果 → JSON 文件
        time_window.h 为计数和休息模块提供区间工具
```

.h 是头文件，放类型和可复用函数；program.cpp 是主程序入口。
#pragma once 防止同一头文件在一次编译中重复展开。#include 引入所需声明与定义。
struct 把相关数据和函数放在一起；对象名.成员名访问对象里的内容。
本项目只有一个主 cpp，不应分别运行各个 h。头文件中独立定义的函数使用 inline，结构体内定义的成员函数也隐式 inline。

## program.cpp：组织整个程序

辅助函数 nowSeconds 取单调时钟；duration 将秒数显示成分:秒；decimal 控制小数位数；label 写一段文字；drawRow 写一行并把 y 下移；sleepyFace 画静音哈欠图案。

main 的各部分按执行顺序是：

1. 读取参数，确定摄像头、模型和 JSON 路径。
2. 创建 EyeDetector、打开摄像头、设置画面大小。
3. 创建 eyes、baseline、rest，分别保存活动记录、个人参考、休息记录。
4. 循环读 frame，调用 detect 得到 eye；计算距上一帧的实际时间及 FPS。
5. 自检模式读取指定帧数后退出；正常模式更新会话时间，非暂停时依次更新活动、离开状态、个人参考。
6. 所有采集模式均检查静音休息提醒；仅在 --preview 模式绘制画面和读取窗口按键。
7. 所有正常采集模式每 5 秒调用 writeMetricsJson；Ctrl+C 或预览退出时保存最终结果并释放摄像头，只有预览模式需要关闭窗口。

session 是启动/重置以来经过的真实秒数，包含暂停，保证“最近 20 分钟”按真实时间过期。暂停不新增检测活动或休息。
frame 是摄像头图片；eye 是当前识别结果；canvas 是带统计面板的显示画布；lastJson 控制写文件节奏。

以下按键仅在 --preview 的摄像头窗口中有效；后台模式使用终端 Ctrl+C 停止。

| 按键 | 作用 |
|---|---|
| 空格 | 开始/结束主动休息 |
| P | 暂停/继续；暂停不算休息 |
| S | 延后提醒：正式模式 120 秒 |
| R | 重置次数、时间、基准和休息记录 |
| D | 显示/隐藏检测细节 |
| Q / Esc | 正常退出；重新编译前先关闭窗口 |

## eye_detector.h：图片识别接口

| 部分 | 作用 |
|---|---|
| EyeResult | 返回本帧是否可靠、两眼闭合分数、张嘴分数、粗略头部角度、视线偏移标记及显示用眼部点 |
| pointDistance2D | 计算两个关键点在图片上的距离 |
| eyeEAR2D | 用六个眼部点算几何开合比例；用于可靠性检查，不是当前主要眨眼计数依据 |
| eyeGazeRatio | 估计虹膜在眼裂内的相对水平和垂直位置 |
| EyeDetector 构造函数 | 读取三个模型，设为 CPU 推理 |
| detect | 找脸 → 缩放旋转裁剪 → 478 点网格 → 粗略姿态/眼部几何检查 → 146 点表情输入 → 闭眼与张嘴分数 → 视线代理判断 |

cv::Mat 是 OpenCV 的图片/矩阵类型。mesh 和 blend 是已训练好的网络，不是本程序重新训练的模型。
blinkLeft/blinkRight 越大表示闭合表达越强，jawOpen 越大表示张嘴表达越强；它们不是医学概率。
固定关键点编号、模型输入尺寸和预处理要匹配模型，不能为缩短代码随意删改。
低头角度不会直接停止眨眼统计；头部偏移仍可依下面的休息规则计入休息。

## eye_activity.h：把连续帧变成事件

| 部分 | 作用 |
|---|---|
| 计数与状态成员 | blinks、partialBlinks、yawns 保存次数；closed、armed、candidate 等记录一次动作走到哪一步 |
| 时间成员 | totalClosed 是累计闭眼秒数；currentClosed 是当前闭眼秒数；longClosed 是已结束长闭眼累计秒数 |
| blinkEvents / yawnEvents | 两个标准库双向链表，分别保存最近一分钟眨眼和哈欠的确认时间 |
| totalValidSeconds | 累计可靠观测秒数，保留完整会话，不随滚动窗口清理 |
| validTime | 保存可靠观测区间，作为频率分母 |
| longClosedTime / currentIntervals | 已完成与正在进行的长闭眼可靠区间，用于休息去重 |
| finishClosed | 结束当前闭眼，达一秒时保存为长闭眼 |
| finishCandidate | 重新睁眼后判断是否增加一次眨眼，并向链表尾部加入时间 |
| pause | 结束或取消当前过程，断开连续性，防止跨暂停计数 |
| restSpans | 返回可参与休息统计的长闭眼时间段 |
| sessionLongClosed | 返回已结束加上当前符合条件的长闭眼秒数，用于界面 |
| shortRate / rate / yawnRate | 共用窗口质量检查，分别计算最近一分钟眨眼和哈欠频率 |
| cumulativeRate / baselineBlinkRate / baselineYawnRate | 用完整会话的总次数和可靠观测时长计算两种累计平均频率 |
| updateYawn | 跟踪张嘴/闭嘴过程，识别连续哈欠并防止同一次重复计数 |
| update | 每帧调用入口：检查连续性、更新哈欠、处理跟踪丢失、判断眼部状态、累加时间、结束候选并清理旧记录 |

眨眼流程：先看到睁眼 → 部分或明确闭合 → 再睁眼，才可能计一次。候选需持续大于零且小于一秒，并且跟踪连续。partialBlinks 是 blinks 的子集，不能再相加。
明确闭合条件为两眼分数平均≥0.55、每眼≥0.35；明确睁眼为平均≤0.30、最大≤0.45；部分候选为平均≥0.35、每眼≥0.20。
开闭采用不同阈值，避免分数在边界抖动时重复触发。持续闭眼达一秒归入长闭眼，不计普通眨眼。

两张可靠图片相隔不超过 0.25 秒才累计时间；丢失超过 0.35 秒结束旧过程。开闭转换发生在两帧之间，用两帧中点估计时间边界。缺失帧不会直接增加闭眼秒数。
哈欠先要求闭嘴，张嘴达到 0.55 后连续保持开放至少一秒，最后降到 0.25 以下闭嘴才计一次。闭嘴立即允许下一次，无 15 秒冷却；持续超过十秒仍不计。中间状态沿用张嘴状态，丢失跟踪/长断帧/暂停中断候选。

## personal_baseline.h：自动建立头部参考

PersonalBaseline 现在只保留头部方向参考，不再学习固定的一分钟眨眼基准。两类事件的累计 baseline 由 EyeActivity 计算。

| 部分 | 作用 |
|---|---|
| referenceSeconds / pitchSum / yawSum | 前十秒有效非休息姿态的时间与加权和 |
| referencePitch / referenceYaw / referenceReady | 自动学习到的参考方向和是否就绪 |
| lastTime / previousValid / previousResting | 排除暂停、缺失和休息区间，保证姿态参考由连续有效观测建立 |
| pause | 打断连续观测 |
| isHeadAway | 相对已学习方向任一角度变化超过 30 度时返回 true |
| update | 学习最初累计 10 秒的有效非休息头部姿态 |

无需额外标定按键；R 重新学习头部参考并重置所有统计。

## time_window.h：通用时间区间工具

| 部分 | 作用 |
|---|---|
| TimeSpan | 一段时间的 begin 和 end |
| earlierStart | 比较两个区间谁先开始，供 sort 排序 |
| coveredSeconds | 裁剪到统计窗口，排序、合并重叠区间，返回总秒数 |
| IntervalSeries::spans | 按顺序保存区间的 vector |
| add | 添加新区间，能与末尾连接时直接合并 |
| trim | 删除二十分钟前的记录，并裁剪跨边界区间 |
| seconds | 返回最近指定窗口内的去重时长 |

例如闭眼 [0,10]、移开视线 [5,15] 的并集是 [0,15]，结果 15 秒，不是 20 秒。
vector 像可变长度数组，适合排序；眨眼时间使用链表，适合尾部添加、头部删除。

## rest_policy.h：休息记录和提醒

| 部分 | 作用 |
|---|---|
| testMode / window / target / snoozeSeconds | 测试/正式模式参数 |
| declaredBreaks / manual / manualStart | 保存主动休息区间和当前状态 |
| awayBreaks / awayCandidate / awayActive / awayStart | 保存离开/偏移区间及一秒确认状态 |
| toggleManual | 按空格时切换主动休息 |
| snooze | 延后提醒 |
| pauseAway | 暂停时结束已确认区间、取消未确认区间 |
| updateAway | 连续一秒离开/偏移后确认休息，补记起始点；返回后结束；长断帧不补记未知间隔 |
| restSpans | 收集长闭眼、主动休息和离开休息区间 |
| recordedRest | 求这些区间在指定窗口内的并集长度，默认 1200 秒 |
| View | 保存当前窗口休息时长、覆盖率、缺口及是否提醒等结果 |
| view | 结合窗口是否已满、覆盖率、休息缺口、暂停和延后状态生成提醒 |

当前 testMode=false：提醒使用最近 1200 秒、目标 60 秒、延后 120 秒；首个 20 分钟完成后，休息记录不足目标且覆盖率足够时才提醒。
无论测试模式如何，JSON 的 rest_seconds_last_20_minutes 都固定使用真实 1200 秒。
暂停不增加休息，旧记录仍随实际时间移出窗口。未满二十分钟只统计启动以来的记录。

依用户设定，检测不到眼睛持续一秒也算休息。视线代理还包括：睁眼时两眼虹膜水平相对位置同时低于 0.25/高于 0.75，或垂直同时低于 0.05/高于 0.95；以及相对参考头部方向偏离超过 30 度。该代理可能把漏检误算为休息，需要真人测试；它不能确定实际注视的屏幕位置。

## metrics_json.h：写给队友读取

jsonNumber 把可用数值写成数字，-1 等无效值写成 null。
writeMetricsJson 创建输出目录，完整写入同目录的 .tmp 文件，关闭检查成功后再替换正式文件，避免队友读到半截 JSON。
std::ofstream 是写文件工具；fixed/setprecision(3) 控制三位小数；Windows 下 MoveFileExW 负责替换已有文件。
七项数据的五秒写入节奏由 program.cpp 的 lastJson 控制，不在这个头文件里等待。

## 上传内容与依赖

上传 program1 下的 program.cpp 和七个 h、README.md、build.ps1、run.ps1、.vscode、.gitignore、third_party。
可执行文件、program1/build 和运行数据被 .gitignore 排除。program1/models 中的三个必需 ONNX 模型及 FACE_MESH_LICENSE.txt、YUNET_LICENSE.txt 已列入上传白名单；三份模型合计约 6.2 MB，建议一起上传，队友克隆后无需另行下载。下方链接用于核对来源或缺失时重新下载。third_party 中的许可证和来源说明也应保留。
已经删除旧版 face.xml、lbfmodel.yaml，以及不再使用的 load_score.h；当前没有代码依赖它们。

| 必需模型文件 | 下载地址 |
|---|---|
| yunet.onnx | https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx |
| face_mesh.onnx | https://github.com/yakhyo/mediapipe-face-mesh-onnx/releases/download/weights/face_landmarker_Nx3x256x256.onnx |
| face_blendshapes.onnx | https://huggingface.co/fernandotonon/QtMeshEditor-faceblendshapes-onnx/resolve/main/face_blendshapes.onnx |

模型来源与许可证：

- Google MediaPipe Face Landmarker：https://developers.google.com/edge/mediapipe/solutions/vision/face_landmarker
- 146 点输入顺序及 52 项表情定义：https://github.com/google-ai-edge/mediapipe/blob/master/mediapipe/tasks/cc/vision/face_landmarker/face_blendshapes_graph.cc
- 面部网格 ONNX 转换：https://github.com/yakhyo/mediapipe-face-mesh-onnx
- 表情 ONNX 转换：https://huggingface.co/fernandotonon/QtMeshEditor-faceblendshapes-onnx
- YuNet：https://github.com/opencv/opencv_zoo/tree/main/models/face_detection_yunet

许可证文本保留在 third_party。AI assistance: OpenAI Codex（实现、整理、测试和环境配置）。预训练模型来自外部项目；计数阈值和休息规则是原型设置，不是医学疲劳诊断标准。哈欠候选也可能是其他持续张嘴动作。



## 正式休息提醒与注释

已恢复二十分钟窗口和六十秒休息目标。正常后台模式也调用提醒判断，使用 Windows 通知弹出静音提示，不打开摄像头预览，不等待用户点击，因此不中断检测或五秒 JSON 写入。同一运行期间，两次成功通知至少间隔 1200 秒；暂停、正在休息、数据不足或休息目标已满足时不触发。R 重置提醒时钟。正常退出删除通知图标。

rest_reminder.h 负责通知节流、隐藏的消息接收窗口和 Windows Shell 通知 API，链接时需要 -lshell32 -luser32（build.ps1 与 VS Code 设置已更新）。Windows 关闭通知或启用勿扰模式时，系统可能不展示通知横幅。接口设置 NIIF_NOSOUND，不播放提醒声音。

按用户要求，program.cpp 的现有注释保留；每个头文件只保留一条英文文件用途注释。

通知 API 参考：https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataw


## 本模块疲劳指数 E（C++ 比赛原型）

eye_fatigue.h 使用现有统计对象，每五秒在原始 JSON 旁写入 blink_fatigue.json。
若通过 --json 更改原始文件名，评分文件名自动改为“原始文件名去掉扩展名 + _fatigue.json”。
原始七个字段不变。fatigue_index 范围 0–1，越高表示疲劳线索越强；0 仅表示此模型没有净增加的疲劳线索，不保证健康或清醒。

令 b、b0 分别为最近一分钟眨眼频率、启动以来的眨眼 baseline；y、y0 为对应哈欠频率，单位均为次/分钟。
两个 baseline 均采用：累计次数 × 60 / 累计可靠观测秒数。
这是项目约定的累计平均定义，并非研究建立的健康基准；初始高负荷和持续疲劳都可能进入 baseline。

眨眼分量：B = max(0, b - b0) / (b + b0)，要求 b0 > 0。

依据：Yin 等（2022）23 人的两小时屏幕任务中，眨眼次数随视觉疲劳进程显著增加。
这支持采用增加方向作为线索，但不证明当前分式或一分钟窗口具有医学效度。分式是本项目把变化限制在 0–1 的设计。
研究：https://pubmed.ncbi.nlm.nih.gov/35161662/
阅读任务本身也能降低眨眼频率，故下降不直接计为疲劳增加：
https://pubmed.ncbi.nlm.nih.gov/36763349/

哈欠分量：Y = max(0, y - y0) / (y + y0 + 1)。

依据：Jie 等（2018）12 人的驾驶模拟研究中，睡眠剥夺状态的哈欠频率显著高于清醒状态，支持其作为辅助线索。
研究没有规定此分式；分母的 1 次/分钟是工程平滑参数，避免 y0 为零时一次事件就达到最高分。
研究：https://www.cl.cam.ac.uk/~pr10/publications/fg18.pdf
本程序检测的是持续张嘴至少一秒后闭嘴的候选事件，说话也可能误触发，不等同于该论文经过验证的分类器。

休息恢复加分：C = 0.2 × min(1, rest_seconds_last_20_minutes / 60)。

依据范围：休息是本项目的恢复行为指标。20 分钟内累计 60 秒、最多恢复 0.2 均为产品设计参数，没有对应研究证明此数值映射。
AOA 的 20-20-20 建议是每 20 分钟看 20 英尺外 20 秒，并不等同于本程序统计的累计闭眼/移开视线时间，也不证明此加分公式。
参考：https://www.aoa.org/healthy-eyes/eye-and-vision-conditions/computer-vision-syndrome
恢复分随最近二十分钟记录计算；休息满六十秒后不继续增加，不与累计闭眼秒数重复计算。

最终：E = clamp(0.7 × B + 0.3 × Y - C, 0, 1)。

依据范围：眨眼与哈欠研究支持多种线索互补，但 0.7、0.3、0.2 的组合是本项目原型参数，没有经过标注数据验证，不是研究原式或疲劳概率。
恢复加分通过降低疲劳指数实现。累计闭眼总时长包含普通眨眼和主动休息，不再另加疲劳分。
不得将该指数用于驾驶或其他安全决策。

任意频率缺失、非有限或为负，或眨眼 baseline 为零时，指数为 null，status 为 insufficient_data；不把缺失当零疲劳。
初始十秒可靠观测不足时频率本来就是 null；首分钟短期频率与累计 baseline 常相同，因此零分不能解释为已建立正常基准。
累计平均会逐渐吸收近期变化；切换任务也会改变频率。本算法只反映当前相对本次会话的变化。
两个 JSON 各自完整替换，但并非一个联合事务；队友读取时应检查两份文件是否都在持续更新。
