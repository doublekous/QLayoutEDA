#!/usr/bin/env python3
"""
GDS 测试文件生成器
生成用于测试的各种 GDS 文件
"""

import struct
import os

def write_gds_record(stream, record_type, data=b''):
    """写入 GDS 记录"""
    length = len(data) + 2
    stream.write(struct.pack('>H', length))
    stream.write(struct.pack('B', record_type))
    stream.write(data)

def create_minimal_gds(filepath):
    """创建最小有效 GDS 文件"""
    with open(filepath, 'wb') as f:
        # HEADER
        write_gds_record(f, 0x00, struct.pack('>H', 600))
        # BGNLIB
        write_gds_record(f, 0x01, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # LIBNAME
        write_gds_record(f, 0x02, b'MINIMAL')
        # UNITS
        write_gds_record(f, 0x03, struct.pack('>dd', 1e-9, 1e-6))
        # ENDLIB
        write_gds_record(f, 0x04)

def create_boundary_gds(filepath):
    """创建包含边界元素的 GDS 文件"""
    with open(filepath, 'wb') as f:
        # HEADER
        write_gds_record(f, 0x00, struct.pack('>H', 600))
        # BGNLIB
        write_gds_record(f, 0x01, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # LIBNAME
        write_gds_record(f, 0x02, b'BOUNDARY_TEST')
        # UNITS
        write_gds_record(f, 0x03, struct.pack('>dd', 1e-9, 1e-6))
        
        # BGNSTR - 开始结构
        write_gds_record(f, 0x05, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # STRNAME
        write_gds_record(f, 0x06, b'TOP')
        
        # BOUNDARY
        write_gds_record(f, 0x08)
        # LAYER
        write_gds_record(f, 0x0D, struct.pack('>H', 1))
        # DATATYPE
        write_gds_record(f, 0x0E, struct.pack('>H', 0))
        # XY - 5 points (closed polygon)
        xy_data = struct.pack('>5ii', 0, 0, 1000, 0, 1000, 1000, 0, 1000, 0, 0)
        write_gds_record(f, 0x10, xy_data)
        # ENDEL
        write_gds_record(f, 0x11)
        
        # ENDSTR
        write_gds_record(f, 0x07)
        
        # ENDLIB
        write_gds_record(f, 0x04)

def create_path_gds(filepath):
    """创建包含路径元素的 GDS 文件"""
    with open(filepath, 'wb') as f:
        # HEADER
        write_gds_record(f, 0x00, struct.pack('>H', 600))
        # BGNLIB
        write_gds_record(f, 0x01, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # LIBNAME
        write_gds_record(f, 0x02, b'PATH_TEST')
        # UNITS
        write_gds_record(f, 0x03, struct.pack('>dd', 1e-9, 1e-6))
        
        # BGNSTR
        write_gds_record(f, 0x05, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # STRNAME
        write_gds_record(f, 0x06, b'TOP')
        
        # PATH
        write_gds_record(f, 0x09)
        # LAYER
        write_gds_record(f, 0x0D, struct.pack('>H', 2))
        # DATATYPE
        write_gds_record(f, 0x0E, struct.pack('>H', 0))
        # PATHTYPE
        write_gds_record(f, 0x21, struct.pack('>H', 0))
        # WIDTH
        write_gds_record(f, 0x0F, struct.pack('>i', 100))
        # XY - 3 points
        xy_data = struct.pack('>3ii', 0, 0, 2000, 0, 2000, 1000)
        write_gds_record(f, 0x10, xy_data)
        # ENDEL
        write_gds_record(f, 0x11)
        
        # ENDSTR
        write_gds_record(f, 0x07)
        
        # ENDLIB
        write_gds_record(f, 0x04)

def create_text_gds(filepath):
    """创建包含文本元素的 GDS 文件"""
    with open(filepath, 'wb') as f:
        # HEADER
        write_gds_record(f, 0x00, struct.pack('>H', 600))
        # BGNLIB
        write_gds_record(f, 0x01, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # LIBNAME
        write_gds_record(f, 0x02, b'TEXT_TEST')
        # UNITS
        write_gds_record(f, 0x03, struct.pack('>dd', 1e-9, 1e-6))
        
        # BGNSTR
        write_gds_record(f, 0x05, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # STRNAME
        write_gds_record(f, 0x06, b'TOP')
        
        # TEXT
        write_gds_record(f, 0x0C)
        # LAYER
        write_gds_record(f, 0x0D, struct.pack('>H', 3))
        # TEXTTYPE
        write_gds_record(f, 0x16, struct.pack('>H', 0))
        # XY
        xy_data = struct.pack('>ii', 500, 500)
        write_gds_record(f, 0x10, xy_data)
        # STRING
        write_gds_record(f, 0x19, b'LABEL')
        # ENDEL
        write_gds_record(f, 0x11)
        
        # ENDSTR
        write_gds_record(f, 0x07)
        
        # ENDLIB
        write_gds_record(f, 0x04)

def create_sref_gds(filepath):
    """创建包含结构引用的 GDS 文件"""
    with open(filepath, 'wb') as f:
        # HEADER
        write_gds_record(f, 0x00, struct.pack('>H', 600))
        # BGNLIB
        write_gds_record(f, 0x01, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # LIBNAME
        write_gds_record(f, 0x02, b'SREF_TEST')
        # UNITS
        write_gds_record(f, 0x03, struct.pack('>dd', 1e-9, 1e-6))
        
        # 被引用的结构
        write_gds_record(f, 0x05, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        write_gds_record(f, 0x06, b'CELL_A')
        write_gds_record(f, 0x08)
        write_gds_record(f, 0x0D, struct.pack('>H', 1))
        write_gds_record(f, 0x0E, struct.pack('>H', 0))
        xy_data = struct.pack('>4ii', 0, 0, 500, 0, 500, 500, 0, 500)
        write_gds_record(f, 0x10, xy_data)
        write_gds_record(f, 0x11)
        write_gds_record(f, 0x07)
        
        # 顶层结构
        write_gds_record(f, 0x05, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        write_gds_record(f, 0x06, b'TOP')
        
        # SREF
        write_gds_record(f, 0x0A)
        write_gds_record(f, 0x12, b'CELL_A')
        write_gds_record(f, 0x10, struct.pack('>ii', 1000, 1000))
        write_gds_record(f, 0x11)
        
        write_gds_record(f, 0x07)
        write_gds_record(f, 0x04)

def create_aref_gds(filepath):
    """创建包含阵列引用的 GDS 文件"""
    with open(filepath, 'wb') as f:
        # HEADER
        write_gds_record(f, 0x00, struct.pack('>H', 600))
        # BGNLIB
        write_gds_record(f, 0x01, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # LIBNAME
        write_gds_record(f, 0x02, b'AREF_TEST')
        # UNITS
        write_gds_record(f, 0x03, struct.pack('>dd', 1e-9, 1e-6))
        
        # 被引用的结构
        write_gds_record(f, 0x05, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        write_gds_record(f, 0x06, b'UNIT')
        write_gds_record(f, 0x08)
        write_gds_record(f, 0x0D, struct.pack('>H', 1))
        write_gds_record(f, 0x0E, struct.pack('>H', 0))
        xy_data = struct.pack('>4ii', 0, 0, 100, 0, 100, 100, 0, 100)
        write_gds_record(f, 0x10, xy_data)
        write_gds_record(f, 0x11)
        write_gds_record(f, 0x07)
        
        # 顶层结构
        write_gds_record(f, 0x05, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        write_gds_record(f, 0x06, b'TOP')
        
        # AREF - 3x2 阵列
        write_gds_record(f, 0x0B)
        write_gds_record(f, 0x12, b'UNIT')
        write_gds_record(f, 0x13, struct.pack('>HH', 3, 2))  # columns, rows
        # 原点, 列向量, 行向量
        xy_data = struct.pack('>3ii', 0, 0, 200, 0, 0, 200)
        write_gds_record(f, 0x10, xy_data)
        write_gds_record(f, 0x11)
        
        write_gds_record(f, 0x07)
        write_gds_record(f, 0x04)

def create_multi_layer_gds(filepath):
    """创建多层 GDS 文件"""
    with open(filepath, 'wb') as f:
        # HEADER
        write_gds_record(f, 0x00, struct.pack('>H', 600))
        # BGNLIB
        write_gds_record(f, 0x01, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # LIBNAME
        write_gds_record(f, 0x02, b'MULTI_LAYER')
        # UNITS
        write_gds_record(f, 0x03, struct.pack('>dd', 1e-9, 1e-6))
        
        # BGNSTR
        write_gds_record(f, 0x05, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        write_gds_record(f, 0x06, b'TOP')
        
        # 多层边界
        for layer in range(1, 6):
            write_gds_record(f, 0x08)
            write_gds_record(f, 0x0D, struct.pack('>H', layer))
            write_gds_record(f, 0x0E, struct.pack('>H', 0))
            offset = layer * 1000
            xy_data = struct.pack('>5ii', 
                offset, offset, 
                offset + 500, offset, 
                offset + 500, offset + 500, 
                offset, offset + 500, 
                offset, offset)
            write_gds_record(f, 0x10, xy_data)
            write_gds_record(f, 0x11)
        
        write_gds_record(f, 0x07)
        write_gds_record(f, 0x04)

def create_large_coords_gds(filepath):
    """创建大坐标 GDS 文件"""
    with open(filepath, 'wb') as f:
        # HEADER
        write_gds_record(f, 0x00, struct.pack('>H', 600))
        # BGNLIB
        write_gds_record(f, 0x01, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        # LIBNAME
        write_gds_record(f, 0x02, b'LARGE_COORDS')
        # UNITS
        write_gds_record(f, 0x03, struct.pack('>dd', 1e-9, 1e-6))
        
        write_gds_record(f, 0x05, struct.pack('>12H', 2026, 3, 19, 23, 0, 0, 2026, 3, 19, 23, 0, 0))
        write_gds_record(f, 0x06, b'TOP')
        
        # 大坐标边界
        write_gds_record(f, 0x08)
        write_gds_record(f, 0x0D, struct.pack('>H', 1))
        write_gds_record(f, 0x0E, struct.pack('>H', 0))
        # 使用大坐标值
        large_val = 10000000000  # 10mm in nm
        xy_data = struct.pack('>5qq', 
            0, 0, 
            large_val, 0, 
            large_val, large_val, 
            0, large_val, 
            0, 0)
        write_gds_record(f, 0x10, xy_data)
        write_gds_record(f, 0x11)
        
        write_gds_record(f, 0x07)
        write_gds_record(f, 0x04)

def main():
    output_dir = os.path.dirname(os.path.abspath(__file__))
    
    files = [
        ('minimal.gds', create_minimal_gds),
        ('boundary.gds', create_boundary_gds),
        ('path.gds', create_path_gds),
        ('text.gds', create_text_gds),
        ('sref.gds', create_sref_gds),
        ('aref.gds', create_aref_gds),
        ('multi_layer.gds', create_multi_layer_gds),
        ('large_coords.gds', create_large_coords_gds),
    ]
    
    for filename, creator in files:
        filepath = os.path.join(output_dir, filename)
        creator(filepath)
        size = os.path.getsize(filepath)
        print(f"Created: {filename} ({size} bytes)")
    
    print(f"\nTotal: {len(files)} test files created")

if __name__ == '__main__':
    main()