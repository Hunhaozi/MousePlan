# MousePlan_Server

MousePlan 服务端，提供：

- 鉴权（注册/登录相关）
- 数据同步（用户、计划、记录）
- 客户端更新检查与安装包下载

## 启动

```bash
npm install
npm start
```

服务默认读取当前目录 `.env`。

## 更新接口

客户端 `checkForUpdates()` 调用：

- `GET /app/update/latest`

返回字段：

- `success`：是否成功
- `latestVersion`：最新版本号（字符串）
- `changelog`：更新日志
- `apkPath`：相对下载路径，例如 `/downloads/MousePlan_latest.apk`
- `apkUrl`：绝对下载地址
- `packageExists`：安装包是否存在
- `packageName`：安装包文件名
- `packageDir`：安装包目录绝对路径

下载路由：

- `GET /downloads/<APK_FILE_NAME>`

## APK 放置路径

默认路径：

- `MousePlan_Server/data/updates/MousePlan_latest.apk`

由 `.env` 控制：

- `APP_UPDATE_PACKAGE_DIR=data/updates`
- `APP_UPDATE_PACKAGE_NAME=MousePlan_latest.apk`
- `APP_LATEST_VERSION=1.00`
- `APP_UPDATE_CHANGELOG=`

## 发布新版本步骤

1. 把新 APK 放到 `APP_UPDATE_PACKAGE_DIR` 指定目录。
2. 文件名与 `APP_UPDATE_PACKAGE_NAME` 保持一致。
3. 更新 `.env` 的 `APP_LATEST_VERSION`（和 `APP_UPDATE_CHANGELOG` 可选）。
4. 重启服务。
5. 浏览器访问 `/app/update/latest` 验证返回。

## 常见问题

- 只替换 APK 不改版本号：
	客户端可能提示“当前已经为最新版本”，不会触发下载。
- `packageExists=false`：
	说明安装包路径或文件名不匹配。
