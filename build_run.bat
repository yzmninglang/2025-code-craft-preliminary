@echo off
REM 设置工作目录为当前脚本所在目录
chcp 65001 >nul
cd /d %~dp0

REM 定义基础路径
set MAIN_FOLDER=.\demos\cpp\main_version
set INTERACTOR=.\interactor\windows\interactor.exe
set INPUT_FILE=.\data\sample_practice1.in
set PYTHON_SCRIPT=.\run.py

REM 查找当前目录下以 main_v 开头且无后缀的文件
setlocal enabledelayedexpansion
set FOUND_MAIN=
for %%F in (main_v*) do (
    REM 检查是否无后缀
    if "%%~xF"=="" (
        REM 提取文件名中的数字部分
        set FILE_NAME=%%F
        set FILE_NUMBER=!FILE_NAME:~6!
        REM 构造对应的 .cpp 文件路径
        set CPP_SOURCE=%MAIN_FOLDER%\main_v!FILE_NUMBER!.cpp
        REM 检查 .cpp 文件是否存在
        if exist "!CPP_SOURCE!" (
            set FOUND_MAIN=!CPP_SOURCE!
            goto :found_main
        )
    )
)

REM 如果未找到符合条件的文件
if not defined FOUND_MAIN (
    echo 错误: 当前目录下未找到符合条件的 main_v? 文件。
    pause
    exit /b 1
)

:found_main
REM 设置最终的 CPP_SOURCE
set CPP_SOURCE=%FOUND_MAIN%

REM 编译 C++ 文件
echo 正在编译 %CPP_SOURCE%...
g++ "%CPP_SOURCE%" -o "%MAIN_FOLDER%\main.exe"
if %ERRORLEVEL% neq 0 (
    echo 错误: 编译失败，请检查 %CPP_SOURCE% 文件。
    pause
    exit /b 1
)
echo 编译成功: main.exe 已生成。

REM 检查 Python 是否可用
where python >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo 错误: Python 未找到，请确保已安装并添加到 PATH 中。
    pause
    exit /b 1
)

REM 运行 Python 命令
echo 正在运行 Python 脚本...
python "%PYTHON_SCRIPT%" "%INTERACTOR%" "%INPUT_FILE%" "%MAIN_FOLDER%\main.exe" -d -r 86356
if %ERRORLEVEL% neq 0 (
    echo 错误: Python 脚本运行失败。
    pause
    exit /b 1
)

echo 完成！
pause