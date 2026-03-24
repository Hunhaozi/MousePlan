# Mouse Plan 项目说明

## 项目简介

Mouse Plan 是一款基于 Qt 和 Android 的健身计划管理软件，旨在帮助用户高效管理健身任务和记录。项目的初衷是为了创建一个契合个人需求的健身应用，避免现有软件的会员模式或功能限制。通过 AI 的协助(~~其实全是AI~~)，项目在短时间内完成了第一代测试版本

## 软件架构

- **入口文件**：
  - `mainwindow_bootstrap.cpp`：软件的整体入口，负责初始化与启动流程。
  - `main.cpp`：预加载模块，加载必要的资源与配置。
- **数据结构**：
  - 所有数据结构均定义在 `appdata` 文件中，包含用户、计划、记录等核心数据。
- **界面流程**：
  - 软件启动后进入预加载界面，随后进入登录界面。
  - 登录成功后进入主界面，新用户会先进入主题选择界面，根据选择进入不同的 Home 界面。

## 环境要求

- **Qt 版本**：推荐使用 Qt 5.12.8。
- **Android 环境**：
  - Android SDK：`platforms;android-29`。
  - Android NDK：`ndk;21.1.6352462`。
  - Java JDK：版本 8 或 11。
- **开发工具**：
  - Qt Creator。
  - Node.js 和 npm（用于服务端部署）。

## 主题功能

Mouse Plan 支持多种主题，不同主题提供不同的功能与流程。

### 健身主题

健身主题是 Mouse Plan 的默认主题，也是应用的核心功能模块。以下是健身主题的功能说明：

#### 本地模式

1. **总计划功能**：
   - 支持设置单个或多个健身总计划。
   - 每个总计划包含练习周期、每日训练内容（动作名称、热身组与正式组的具体内容）、开始时间与休息日等。

2. **日历映射功能**：
   - 可选择某个总计划为当前计划。
   - 当前计划的内容会映射到日历组件上，用户可通过点击日历查看未来计划。
   - 当天的计划内容会直接预览。

3. **打卡功能**：
   - 用户可勾选当天的每个项目表示完成。
   - 所有项目完成后可提交打卡记录，保存当天的训练记录与时间。

4. **总计划导出/导入**：
   - 支持总计划数据包的导出与导入，防止因特殊原因导致总计划丢失。

5. **扩展功能**（待实现）：
   - 补录、间歇时间提醒、食物热量/营养计算、体重记录、总数据备份等。

#### 在线模式

在本地模式的基础上，在线模式提供以下功能：

1. **注册登录**：
   - 用户可注册并登录，保留专属 ID。
   - 支持在不同设备上登录并同步数据。

2. **数据保存**：
   - 打卡后，相关数据会更新至服务器。
   - 即便卸载软件或更换设备，数据也不会丢失。

3. **在线计划**（待实现）：
   - 用户可分享或上传自己的总计划至服务器，供他人下载借鉴。

> 注：在线模式仅在打卡时尝试保存数据。登录过一次后，除联网功能外，在线模式可当作本地模式使用。

## 云服务器网络服务

Mouse Plan 提供了简单的服务器端接口，用户可将服务端文件部署到自己的云服务器，并在软件的“服务器设置”界面填入服务器地址。

### 服务端功能

1. **注册码管理**：
   - 支持注册码的生成与验证。

2. **数据同步**：
   - 支持用户数据的上传与下载。

3. **更新检查**：
   - 检查是否有新版本可用。

### 服务端部署方法

1. **环境准备**：
   - 安装 Node.js 和 npm。
   - 确保服务器支持 HTTP 请求。

2. **克隆代码仓库**：

   ```bash
   git clone https://github.com/Hunhaozi/MousePlan.git
   cd MousePlan_Server
   ```

3. **安装依赖**：

   ```bash
   npm install
   ```

4. **配置环境变量**：
   - 创建 `.env` 文件，设置以下内容：

     ```env
     PORT=3000
     DB_PATH=./data/db.json
     ```

5. **启动服务**：

   ```bash
   npm start
   ```

   服务启动后，访问 `http://localhost:3000`，如果配置为自己服务器，那么将该地址直接填入 APP 的服务器设置选项中即可使用。

6. **部署到生产环境**：
   - 使用 PM2 或其他进程管理工具保持服务运行：

     ```bash
     npm install -g pm2
     pm2 start src/index.js --name MousePlan_Server
     ```

   - 配置 Nginx 或 Apache 反向代理以支持 HTTPS。

## APP 使用方法

首先下载APP的安装包(apk)文件，然后直接安装即可，安装包在项目的OBJ/Beta文件夹中,如果需要注册码可以留言或者邮箱联系我，欢迎提供建议和bug

1. **登录**：
   - 本地模式可直接登录使用。
   - 在线模式需使用注册码注册账号。

2. **创建计划**：
   - 进入健身主题(默认)，设置总计划与每日训练内容。

3. **打卡记录**：
   - 完成当天训练后，提交打卡记录。

4. **数据同步**：
   - 在线模式下，打卡记录会自动同步至服务器。

## 待添加

1. 健身单项目的间歇定时器提示
2. 在线总计划的共享
3. 学习主题和普通主题

## 项目地址

- [Github 项目地址](https://github.com/Hunhaozi/MousePlan)
- [blog 项目地址]()

项目架构:
``` c
Mouse_Plan/
|-- main.cpp                         # Qt 应用入口
|-- mainwindow.h                     # MainWindow 类声明
|-- mainwindow.cpp                   # MainWindow 壳文件（实现已迁移到 modules）
|-- mainwindow.ui                    # Qt Designer UI 文件
|-- appdata.h                        # 数据模型与存储接口
|-- appdata.cpp                      # 本地存储与 JSON 序列化
|-- img.qrc                          # Qt 资源索引
|-- Mouse_Plan_2.pro                 # qmake 项目文件
|-- Mouse_Plan_2.pro.user            # Qt Creator 本地用户配置
|-- Mouse_Plan_2.code-workspace      # VS Code 工作区文件
|-- README.md                        # 项目架构文档
|-- .vscode/                         # 本地编辑器配置
|-- config/                          # 全局相关的配置文件
|-- img/                             # 图片资源
|-- android/                         # Android 打包配置--略

|
|-- modules/                                            # 分层应用代码（按 common/themes/ui 拆分）
|   |-- common/                                         # 跨主题通用能力层
|   |   |-- agreement/                                  # 协议文本加载与模式切换（本地/在线）
|   |   |   |-- agreement_text_loader.h                 # 协议加载器接口
|   |   |   \-- agreement_text_loader.cpp               # 协议文件读取与回退逻辑实现
|   |   |-- config/                                     # 公共配置常量
|   |   |   \-- network_config.h                        # 网络地址与接口配置常量
|   |   |-- theme/                                      # 主题策略与可用性控制
|   |   |   |-- theme_feature_gate.h                    # 主题开关/可用性判断接口
|   |   |   |-- theme_feature_gate.cpp                  # 主题可用性拦截实现
|   |   |   |-- theme_strategy_factory.h                # 主题策略工厂接口
|   |   |   \-- theme_strategy_factory.cpp              # 主题策略分发与构建实现
|   |   |-- ui/                                         # 公共 UI 构建与弹窗工具
|   |   |   |-- common_ui_pages.cpp                     # 通用页面片段创建
|   |   |   |-- runtime_dialog_helpers.h                # 运行时弹窗工具接口
|   |   |   \-- runtime_dialog_helpers.cpp              # 运行时弹窗工具实现
|   |   \-- update/                                     # 应用更新辅助能力
|   |       |-- update_client_helper.h                  # 更新客户端接口
|   |       \-- update_client_helper.cpp                # 更新检查与下载辅助实现
|   |           
|   |-- themes/                                         # 主题业务层（按主题拆分）
|   |   \-- fitness/                                    # 健身主题业务
|   |       |-- calendar/                               # 日历标记与日期展示
|   |       |   |-- fitness_calendar_mark_builder.h     # 日历标记构建接口
|   |       |   \-- fitness_calendar_mark_builder.cpp   # 日历训练/休息标记生成
|   |       |-- data/                                   # 健身主题数据模型与仓储
|   |       |   |-- fitness_data_models.h               # 健身业务数据结构
|   |       |   |-- fitness_data_repository.h           # 数据仓储接口
|   |       |   \-- fitness_data_repository.cpp         # 数据读取与聚合实现
|   |       |-- network/                                # 健身主题网络接口定义
|   |       |   |-- fitness_online_api.h                # 在线 API 声明
|   |       |   \-- fitness_online_api.cpp              # 在线 API 常量实现
|   |       \-- plan/                                   # 计划与记录核心流程
|   |           |-- fitness_plan_flow_helper.h          # 计划流程辅助接口
|   |           |-- fitness_plan_flow_helper.cpp        # 计划流程计算实现
|   |           |-- fitness_plan_runtime.cpp            # 计划运行时公共逻辑
|   |           |-- fitness_plan_item_actions.cpp       # 计划项操作（完成/忽略/编辑）
|   |           |-- fitness_training_record_actions.cpp # 训练记录提交与补录
|   |           \-- mainwindow_fitness_plan_manager.cpp # 计划管理页与主窗口联动
|   |
|   \-- ui/                                             # 主窗口交互层（按页面功能拆分）
|       |-- common/                                     # 启动、登录后流程等主线逻辑
|       |   |-- mainwindow_bootstrap.cpp                # MainWindow 初始化与启动流程
|       |   \-- mainwindow_session_flow.cpp             # 会话流程、页面切换与状态同步
|       |-- home/                                       # 首页相关实现预留目录
|       |-- login/                                      # 登录与注册流程
|       |   |-- login_register_flow.h                   # 登录/注册流程接口
|       |   \-- login_register_flow.cpp                 # 登录/注册流程实现
|       |-- profile/                                    # 个人中心相关交互
|       |   |-- profile_interaction_helper.h            # 个人中心交互辅助接口
|       |   |-- profile_interaction_helper.cpp          # 个人中心交互辅助实现
|       |   \-- mainwindow_profile_actions.cpp          # 主窗口个人页动作实现
|       \-- theme/                                      # 主题切换与账号主题行为
|           |-- mainwindow_theme_actions.cpp            # 主窗口主题相关动作实现
|           \-- theme_account_actions.cpp               # 主题与账号联动动作实现
|
|-- MousePlan_Server/                # Node.js 后端服务
|   |-- .env
|   |-- .gitignore
|   |-- .htaccess
|   |-- Mouse_Add_code              #添加注册码的服务器接口
|   |-- package.json
|   |-- package-lock.json
|   |-- README.md                   #使用文档
|   |-- src/
|   |   |-- config.js
|   |   |-- db.js                   
|   |   \-- index.js
|   |-- data/
|   |   |-- .gitkeep
|   |   |-- db.json                 # 服务器数据的json串
|   |   \-- updates/                # APK/更新包投放目录
|   \-- node_modules/               # 已安装 npm 依赖相关
````
