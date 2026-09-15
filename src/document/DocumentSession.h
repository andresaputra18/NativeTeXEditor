#pragma once

#include <QString>

class DocumentSession final
{
public:
    DocumentSession();

    void resetUntitled();
    void setFilePath(const QString& filePath);

    [[nodiscard]] const QString& filePath() const noexcept;
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] bool hasFilePath() const noexcept;

    void setDirty(bool dirty) noexcept;
    [[nodiscard]] bool isDirty() const noexcept;

private:
    QString filePath_;
    bool dirty_ = false;
};
