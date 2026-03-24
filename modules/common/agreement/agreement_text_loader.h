#ifndef AGREEMENT_TEXT_LOADER_H
#define AGREEMENT_TEXT_LOADER_H

#include <QByteArray>
#include <QString>

namespace mouseplan {
namespace common {

// 协议文本加载器：负责路径查找、原始文本解析、模式化协议读取。
class AgreementTextLoader {
public:
    static QString findConfigFilePath(const QString &fileName);
    static QString parseAgreementTextFromRaw(const QByteArray &raw);
    static QString loadAgreementTextByMode(bool isLocalMode);
};

} // namespace common
} // namespace mouseplan

#endif // AGREEMENT_TEXT_LOADER_H
