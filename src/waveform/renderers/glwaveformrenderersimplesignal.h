#ifndef GLWAVEFORMRENDERERSIMPLESIGNAL_H
#define GLWAVEFORMRENDERERSIMPLESIGNAL_H

#include <QT_OPENGL_ES_2>

#include "waveformrenderersignalbase.h"

class ControlObject;

class GLWaveformRendererSimpleSignal : public WaveformRendererSignalBase, protected QT_OPENGL_ES_2 {
public:
    explicit GLWaveformRendererSimpleSignal(WaveformWidgetRenderer* waveformWidgetRenderer);
    virtual ~GLWaveformRendererSimpleSignal();

    virtual void onSetup(const QDomNode &node);
    virtual void draw(QPainter* painter, QPaintEvent* event);
};

#endif // GLWAVEFORMRENDERERSIMPLESIGNAL_H
