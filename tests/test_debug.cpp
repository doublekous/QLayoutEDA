/**
 * @file test_debug.cpp
 * @brief 调试 GDS 解析
 */

#include <iostream>
#include <iomanip>
#include <QFile>
#include <QDataStream>

int main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    
    QString filePath = "D:/testFile/20250530/test22.gds";
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cout << "Cannot open file" << std::endl;
        return 1;
    }
    
    std::cout << "File size: " << file.size() << " bytes" << std::endl;
    std::cout << "\nFirst 50 records:\n" << std::endl;
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    int recordCount = 0;
    while (!stream.atEnd() && recordCount < 50) {
        quint16 recordLength = 0;
        quint16 recordType = 0;
        
        stream >> recordLength;
        stream >> recordType;
        
        if (stream.status() != QDataStream::Ok) {
            std::cout << "Read error at record " << recordCount << std::endl;
            break;
        }
        
        int dataSize = recordLength - 4;
        
        std::cout << "Record " << std::setw(3) << recordCount << ": "
                  << "len=" << std::setw(5) << recordLength 
                  << ", type=0x" << std::hex << std::setw(4) << std::setfill('0') << recordType << std::dec
                  << " (hi=0x" << std::hex << (recordType >> 8) << ", lo=0x" << (recordType & 0xFF) << std::dec << ")";
        
        // 打印数据的前几个字节
        if (dataSize > 0) {
            QByteArray data(dataSize, 0);
            stream.readRawData(data.data(), dataSize);
            
            std::cout << " data=[";
            int printLen = std::min(dataSize, 20);
            for (int i = 0; i < printLen; ++i) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (static_cast<unsigned char>(data[i]) & 0xFF);
                if (i < printLen - 1) std::cout << " ";
            }
            if (dataSize > 20) std::cout << "...";
            std::cout << "]" << std::dec;
        }
        
        std::cout << std::endl;
        
        recordCount++;
    }
    
    file.close();
    return 0;
}