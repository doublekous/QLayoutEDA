# GDS 测试文件集

本目录存放用于测试的 GDS 文件。

## 文件清单

### 基础测试文件

| 文件名 | 大小 | 用途 | 状态 |
|--------|------|------|------|
| minimal.gds | < 1KB | 最小有效 GDS | 待准备 |
| empty.gds | 0KB | 空文件 | 待准备 |
| boundary.gds | < 1KB | 仅包含边界元素 | 待准备 |
| path.gds | < 1KB | 仅包含路径元素 | 待准备 |
| text.gds | < 1KB | 仅包含文本元素 | 待准备 |
| sref.gds | < 1KB | 包含结构引用 | 待准备 |
| aref.gds | < 1KB | 包含阵列引用 | 待准备 |

### 边界情况测试

| 文件名 | 大小 | 用途 | 状态 |
|--------|------|------|------|
| max_coords.gds | < 10KB | 最大坐标值 | 待准备 |
| negative_coords.gds | < 10KB | 负坐标 | 待准备 |
| deep_nesting.gds | < 100KB | 深层嵌套引用 | 待准备 |
| circular_ref.gds | < 10KB | 循环引用 | 待准备 |
| max_layers.gds | < 100KB | 256 层 | 待准备 |

### 性能测试文件

| 文件名 | 大小 | 用途 | 状态 |
|--------|------|------|------|
| small.gds | 1-10MB | 小文件性能 | 待准备 |
| medium.gds | 10-50MB | 中等文件性能 | 待准备 |
| large_50mb.gds | 50MB | 大文件性能 | 待准备 |
| large_100mb.gds | 100MB | 大文件性能 | 待准备 |

### 错误处理测试

| 文件名 | 大小 | 用途 | 状态 |
|--------|------|------|------|
| corrupted.gds | < 1KB | 损坏的记录 | 待准备 |
| truncated.gds | < 1KB | 截断的文件 | 待准备 |
| invalid_layer.gds | < 10KB | 无效图层号 | 待准备 |

## GDSII 文件格式参考

GDSII 是二进制格式，基本结构：

```
HEADER (0x00 0x02)
BGNLIB
LIBNAME
UNITS
...
BGNSTR
STRNAME
...
BOUNDARY
LAYER
DATATYPE
XY
ENDEL
...
ENDSTR
...
ENDLIB
```

## 测试文件来源

1. **公开 GDS 文件**
   - [nanohub.org](https://nanohub.org) - 公开 EDA 资源
   - [gdsii.org](http://gdsii.org) - 格式参考
   
2. **自行生成**
   - 使用 KLayout 导出
   - 使用 Python 脚本生成

3. **项目内部**
   - 设计团队提供的示例文件

## 文件准备任务

- [ ] 从公开资源下载示例 GDS
- [ ] 使用 KLayout 创建基础测试文件
- [ ] 编写 Python 脚本生成边界情况文件
- [ ] 创建损坏文件用于错误处理测试