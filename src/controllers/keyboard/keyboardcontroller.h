#ifndef KEYBOARDCONTROLLER_H
#define KEYBOARDCONTROLLER_H

#include "controllers/controller.h"
#include "controllers/keyboard/keyboardcontrollerpreset.h"
#include "keyboardeventfilter.h"

class KeyboardController : public Controller {
    Q_OBJECT
public:
    KeyboardController(KeyboardEventFilter* pKbdEventFilter);
    virtual ~KeyboardController();

    virtual QString presetExtension() override;

    virtual ControllerPresetPointer getPreset() const override {
        KeyboardControllerPreset* pClone = new KeyboardControllerPreset();
        *pClone = m_preset;
        return ControllerPresetPointer(pClone);
    }

    virtual bool savePreset(const QString fileName) const override;
    virtual bool matchPreset(const PresetInfo& preset) override;
    virtual void visit(const KeyboardControllerPreset* preset) override;
    virtual void visit(const MidiControllerPreset* preset) override;
    virtual void visit(const HidControllerPreset* preset) override;
    virtual void accept(ControllerVisitor* visitor) {
        if (visitor) {
            visitor->visit(this);
        }
    }
    inline virtual bool isMappable() const override { return m_preset.isMappable(); }

public slots:
    // Sets a control, only if the keyboard is enabled
    void onKeySeqPressed(ConfigKey configKey);

private:
    virtual void send(QByteArray data) override {
        // Keyboard is an input-only controller, so this
        // method doesn't need any implementation
        Q_UNUSED(data);
    }

    inline virtual bool isPolling() const override { return false; }
    inline virtual ControllerPreset* preset() override { return &m_preset; }

    KeyboardControllerPreset m_preset;
    KeyboardEventFilter* m_pKbdEventFilter;

private slots:
    virtual int open() override;
    virtual int close() override;
};


#endif // KEYBOARDCONTROLLER_H