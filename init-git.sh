#!/bin/bash
# QLayoutEDA Git 初始化脚本
# 使用方法: ./init-git.sh

echo "=== QLayoutEDA Git 初始化 ==="
echo ""

# 检查是否已是 Git 仓库
if [ -d ".git" ]; then
    echo "⚠️  当前目录已经是 Git 仓库"
    echo "是否要重新初始化？(y/n)"
    read -r answer
    if [ "$answer" != "y" ]; then
        echo "取消初始化"
        exit 0
    fi
    rm -rf .git
fi

# 初始化 Git
echo "📦 初始化 Git 仓库..."
git init

# 配置用户信息（Bot 账号）
echo "👤 配置 Git 用户信息..."
git config user.name "QLayoutEDA-Bot"
git config user.email "bot@qlayout.aida"

# 添加所有文件
echo "📁 添加文件到暂存区..."
git add .

# 创建首次提交
echo "💾 创建初始提交..."
git commit -m "chore: 初始化 QLayoutEDA 项目

- GDS 解析器
- 图形渲染引擎
- UI 界面框架
- 测试框架
- CI/CD 配置
- Issue/PR 模板

[eda_architect]"

# 创建 develop 分支
echo "🌿 创建 develop 分支..."
git checkout -b develop

# 切换回 main
git checkout main

echo ""
echo "✅ Git 初始化完成！"
echo ""
echo "下一步："
echo "1. 在 GitHub 创建仓库: https://github.com/new"
echo "   - 仓库名: QLayoutEDA"
echo "   - 可见性: Public 或 Private"
echo "   - 不要初始化 README"
echo ""
echo "2. 添加远程仓库:"
echo "   git remote add origin https://github.com/YOUR_USERNAME/QLayoutEDA.git"
echo ""
echo "3. 推送代码:"
echo "   git push -u origin main"
echo "   git push -u origin develop"
echo ""
echo "4. 配置 Secrets:"
echo "   在仓库 Settings → Secrets → Actions 添加:"
echo "   - OPENCLAW_WEBHOOK_URL (可选)"