# Aurora Dashboard 編譯指南

## ✅ 代碼修改完成
所有源代碼已成功修改並通過 C++ 編譯檢查。

## 🔧 編譯步驟

### 方案 1：在 Visual Studio IDE 中（推薦）
1. 導航到 `aurora_remote_sdk_demo/build/` 目錄
2. 打開 `Aurora_Remote_SDK_Demo.sln`
3. 在解決方案資源管理器中，找到 **aurora_dashboard** 項目
4. 右鍵單擊 → **Build** （或 Ctrl+B）
5. 等待編譯完成

**輸出位置：** `build/bin/Release/aurora_dashboard.exe`

### 方案 2：使用 CMake（命令列）
```bash
cd aurora_remote_sdk_demo
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --target aurora_dashboard --config Release
```

### 方案 3：使用 Qt Creator（如已安裝）
1. 打開 Qt Creator
2. File → Open File or Project
3. 選擇 `aurora_remote_sdk_demo/CMakeLists.txt`
4. 配置後，Build → Build Project (或 Ctrl+B)

## 📋 先決條件檢查

在編譯前，請確保：

- ✅ Visual Studio 2019/2022 已安裝（MSVC 工具鏈）
- ✅ Qt 5.12.12 可訪問於 `C:/Qt/Qt5.12.12/5.12.12/msvc2017_64`
- ✅ Aurora SDK 庫位於 `aurora_remote_public/lib/win64/`
- ✅ CMake 3.16+ 已安裝

## 🐛 常見問題

### 問題 1：Qt 庫未找到
**錯誤：** `LNK2019` 或類似 `pcre2_config_16` 的未解析符號

**解決方案：**
1. 驗證 Qt 路徑：CMakeLists.txt 第 84 行
2. 確認 Qt5 libs 在 `C:\Qt\Qt5.12.12\5.12.12\msvc2017_64\lib\` 目錄中
3. 如果路徑不同，編輯 CMakeLists.txt 的 `CMAKE_PREFIX_PATH`

### 問題 2：Visual Studio 未找到
**錯誤：** `could not find any instance of Visual Studio`

**解決方案：**
使用 NMake 生成器代替：
```bash
cmake .. -G "NMake Makefiles"
nmake aurora_dashboard
```

### 問題 3：AURORA SDK 庫未找到
**錯誤：** `slamtec_aurora_remote_sdk` 未找到

**解決方案：**
1. 驗證 `aurora_remote_public/lib/` 目錄完整
2. 檢查 Windows/Linux 平台的正確子目錄（win64/linux_x86_64/etc）
3. CMakeLists.txt 應自動檢測平台

## 📦 編譯輸出物

成功編譯後，生成的文件：
- `build/bin/Release/aurora_dashboard.exe` — 主執行檔
- `build/Release/aurora_dashboard_autogen/` — Qt MOC 生成文件
- 所有 Aurora SDK DLL 自動複製到 exe 目錄

## 🚀 運行應用

編譯完成後：
```bash
cd build/bin/Release
./aurora_dashboard.exe
```

或在 Qt Creator 中按 Ctrl+R。

## 📝 修改的文件列表

- `demo/aurora_dashboard/src/SdkWorker.h` — 新增 occupancy map signal/slots
- `demo/aurora_sdk_demo/src/SdkWorker.cpp` — 佔用柵格地圖邏輯
- `demo/aurora_dashboard/src/MapWidget.h` — 新增 QImage 成員
- `demo/aurora_dashboard/src/MapWidget.cpp` — 地圖渲染邏輯  
- `demo/aurora_dashboard/src/MainWindow.h` — 新增 Mapping Control UI
- `demo/aurora_dashboard/src/MainWindow.cpp` — UI 佈局與訊號連接

## ✨ 新功能驗證清單

編譯完成後，測試以下功能：

- [ ] 連線至 Aurora 設備
- [ ] 佔用柵格地圖在 MapWidget 上顯示（灰階）
- [ ] 移動設備，地圖實時更新
- [ ] 點擊 "Start Mapping" 按鈕，狀態更新
- [ ] 點擊 "Stop Mapping" 按鈕，切換模式
- [ ] 點擊 "Reset Map" 按鈕，清除地圖
- [ ] VSLAM 軌跡仍在地圖上方顯示

## 🔗 相關文件

- `PHASE1_2_CHANGES.md` — 詳細實作說明
- `../plan.md` — 完整實作計畫

---

如有編譯問題，請檢查上述錯誤診斷部分，或提供完整的編譯錯誤信息。
