/**
 * @file GDSFormat.h
 * @brief GDSII 格式定义
 */

#pragma once

#include <QtGlobal>
#include <QByteArray>
#include <cmath>

namespace QLayoutEDA {

/**
 * @brief GDSII 记录类型 (根据实际文件验证)
 */
namespace GDSRecord {
    // 库级别记录
    constexpr quint16 HEADER     = 0x0002;  // 版本号
    constexpr quint16 BGNLIB     = 0x0102;  // 库开始时间
    constexpr quint16 LIBNAME    = 0x0206;  // 库名
    constexpr quint16 UNITS      = 0x0305;  // 单位
    constexpr quint16 ENDLIB     = 0x0400;  // 库结束
    
    // 结构级别记录
    constexpr quint16 BGNSTR     = 0x0502;  // 结构开始时间
    constexpr quint16 STRNAME    = 0x0606;  // 结构名
    constexpr quint16 ENDSTR     = 0x0700;  // 结构结束
    
    // 元素类型
    constexpr quint16 BOUNDARY   = 0x0800;  // 边界
    constexpr quint16 PATH       = 0x0900;  // 路径
    constexpr quint16 SREF       = 0x0A00;  // 结构引用
    constexpr quint16 AREF       = 0x0B00;  // 阵列引用
    constexpr quint16 TEXT       = 0x0C00;  // 文本
    constexpr quint16 BOX        = 0x0D00;  // 矩形框
    constexpr quint16 NODE       = 0x0E00;  // 节点
    
    // 属性记录
    constexpr quint16 LAYER      = 0x0D02;  // 层号 (验证: 0x0D02)
    constexpr quint16 DATATYPE   = 0x0E02;  // 数据类型 (验证: 0x0E02)
    constexpr quint16 WIDTH      = 0x0F03;  // 宽度
    constexpr quint16 XY         = 0x1003;  // 坐标 (验证: 0x1003)
    constexpr quint16 ENDEL      = 0x1100;  // 元素结束 (验证: 0x1100)
    
    // 引用属性
    constexpr quint16 SNAME      = 0x1206;  // 引用的结构名
    constexpr quint16 STRANS     = 0x1A04;  // 变换标志
    constexpr quint16 MAG        = 0x1B05;  // 缩放
    constexpr quint16 ANGLE      = 0x1C05;  // 角度
    constexpr quint16 COLROW     = 0x1302;  // 行列数
    
    // 文本属性
    constexpr quint16 TEXTTYPE   = 0x1602;  // 文本类型
    constexpr quint16 PRESENTATION = 0x1701; // 显示方式
    constexpr quint16 STRING     = 0x1906;  // 字符串
    
    // 路径属性
    constexpr quint16 PATHTYPE   = 0x2102;  // 路径类型
    
    // 属性
    constexpr quint16 PROPATTR   = 0x2B02;  // 属性码
    constexpr quint16 PROPVALUE  = 0x2C06;  // 属性值
}

/**
 * @brief GDSII 数据类型（记录高字节低4位）
 */
namespace GDSDataType {
    constexpr quint16 NO_DATA    = 0x0;  // 无数据
    constexpr quint16 BITARRAY   = 0x1;  // 位阵列
    constexpr quint16 INT2       = 0x2;  // 2字节整数
    constexpr quint16 INT4       = 0x3;  // 4字节整数
    constexpr quint16 REAL4      = 0x4;  // 4字节浮点(废弃)
    constexpr quint16 REAL8      = 0x5;  // 8字节浮点
    constexpr quint16 ASCII      = 0x6;  // ASCII字符串
}

/**
 * @brief 读取 GDSII 8字节浮点数
 */
inline double readGDSReal8(const QByteArray& bytes)
{
    if (bytes.size() < 8) return 0.0;
    
    quint8 firstByte = static_cast<quint8>(bytes[0]);
    int sign = (firstByte & 0x80) ? -1 : 1;
    int exponent = (firstByte & 0x7F);
    
    quint64 mantissa = 0;
    for (int i = 1; i < 8; ++i) {
        mantissa = (mantissa << 8) | static_cast<quint8>(bytes[i]);
    }
    
    if (mantissa == 0 && exponent == 0) {
        return 0.0;
    }
    
    double value = sign * static_cast<double>(mantissa);
    int power = exponent - 64 - 14;
    
    if (power >= 0) {
        value *= std::pow(16.0, power);
    } else {
        value /= std::pow(16.0, -power);
    }
    
    return value;
}

} // namespace QLayoutEDA