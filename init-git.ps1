# QLayoutEDA Git 初始化脚本 (PowerShell)
# 使用方法: .\init-git.ps1

Write-Host "=== QLayoutEDA Git 初始化 ===" -ForegroundColor Green
Write-Host ""

# 检查是否已是 Git 仓库
if (Test-Path ".git") {
    Write-Host "⚠️  当前目录已经是 Git 仓库" -ForegroundColor Yellow
    $answer = Read-Host "是否要重新初始化？(y/n)"
    if ($answer -ne "y") {
        Write-Host "取消初始化"
        exit 0
    }
    Remove-Item -Recurse -Force .git
}

# 初始化 Git
Write-Host "📦 初始化 Git 仓库..." -ForegroundColor Cyan
git init

# 配置用户信息（Bot 账号）
Write-Host "👤 配置 Git 用户信息..." -ForegroundColor Cyan
git config user.name "QLayoutEDA-Bot"
git config user.email "bot@qlayout.local"

# 添加所有文件
Write-Host "📁 添加文件到暂存区..." -ForegroundColor Cyan
git add .

# 创建首次提交
Write-Host "💾 创建初始提交..." -ForegroundColor Cyan
git commit -m "chore: 初始化 QLayoutEDA 项目

- GDS 解析器
- 图形渲染引擎
- UI 界面框架
- 测试框架
- CI/CD 配置
- Issue/PR 模板

[eda_architect]"

# 创建 develop 分支
Write-Host "🌿 创建 develop 分支..." -ForegroundColor Cyan
git checkout -b develop

# 切换回 main
git checkout main

Write-Host ""
Write-Host "✅ Git 初始化完成！" -ForegroundColor Green
Write-Host ""
Write-Host "下一步：" -ForegroundColor Yellow
Write-Host "1. 在 GitHub 创建仓库: https://github.com/new"
Write-Host "   - 仓库名: QLayoutEDA"
Write-Host "   - 可见性: Public 或 Private"
Write-Host "   - 不要初始化 README"
Write-Host ""
Write-Host "2. 添加远程仓库:"
Write-Host "   git remote add origin https://github.com/YOUR_USERNAME/QLayoutEDA.git"
Write-Host ""
Write-Host "3. 推送代码:"
Write-Host "   git push -u origin main"
Write-Host "   git push -u origin develop"
Write-Host ""
Write-Host "4. 配置 Secrets:"
Write-Host "   在仓库 Settings → Secrets → Actions 添加:"
Write-Host "   - OPENCLAW_WEBHOOK_URL (可选)"