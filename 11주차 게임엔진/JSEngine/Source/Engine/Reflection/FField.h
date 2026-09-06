#pragma once

class FField
{
public:
    FField() = default;
    explicit FField(const char* InName)
        : Name(InName)
    {
    }

    virtual ~FField() = default;

    const char* GetName() const { return Name; }

protected:
    void SetName(const char* InName) { Name = InName; }

private:
    const char* Name = nullptr;
};
