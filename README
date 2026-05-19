# 审查意见智能提取工具 (ReviewOpinionTools)

这是一个专为处理复杂、非结构化文档（Word/Excel）设计的审查意见智能提取工具。针对海量文件、含有格式污染（幽灵行/超大 UsedRange）的 Excel 表格进行了深度的底层优化，彻底解决了主线程卡死和内存溢出问题。

## 🚀 项目背景
在日常工作中，经常需要从成百上千份格式各异的文档中提取“审查意见”。传统的解析方案在面对以下情况时极易崩溃：
- **Excel 幽灵行陷阱**：用户误操作导致 Excel 实际只有 50 行，但底层判定为 100 万行，导致 C++ 解析器死循环。
- **UI 阻塞**：数据装配在主线程导致界面假死。
- **内存碎片**：频繁的内存分配导致程序在处理大批量文件时 Crash。

本项目通过多线程架构和硬核的底层熔断逻辑，实现了“秒级”处理速度和极高的稳定性。

## ✨ 核心特性
- **防爆解析引擎**：针对 Excel 的 `UsedRange` 维度爆炸问题，内置“物理熔断”与“硬截断”机制，即使 100 万行的空表也能在 0.1 秒内跳出。
- **极速多线程**：采用 QtConcurrent 架构，支持文件并发解析，充分榨干 CPU 多核性能。
- **智能正则过滤**：通过 `rule15` 正则清洗引擎，自动剔除冗余空格、非标字符。
- **极致 UI 性能**：优化了表格渲染逻辑，彻底铲除了 `resizeRowsToContents` 带来的主线程性能肿瘤，即使几十万条数据也能瞬间呈现。

## 🛠️ 技术栈
- **Language**: C++ 17
- **Framework**: Qt 6.x
- **Integration**: Windows COM/OLE 接口

## 📦 如何构建
1. 使用 Visual Studio 2022 打开 `.sln` 解决方案。
2. 配置 Qt MSVC 编译环境。
3. 直接编译生成 Release 版本即可。

## ⚖️ 许可协议 (License)

本项目采用 **MIT 协议** 开源，您可以自由使用、修改及商用，但请保留原作者声明。

This project is licensed under the **MIT License**. You are free to use, modify, and distribute this software for any purpose, with or without fee, provided that the original copyright notice and this permission notice are included in all copies or substantial portions of the software.

---
*If you find this project helpful, feel free to give a star!*