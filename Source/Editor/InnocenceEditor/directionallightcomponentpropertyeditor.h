#ifndef DIRECTIONALLIGHTCOMPONENTPROPERTYEDITOR_H
#define DIRECTIONALLIGHTCOMPONENTPROPERTYEDITOR_H

#include <QLabel>
#include <QWidget>
#include <QGridLayout>
#include <QDoubleValidator>
#include "icomponentpropertyeditor.h"
#include "combolabeltext.h"
#include "../../Engine/Component/DirectionalLightComponent.h"

class DirectionalLightComponentPropertyEditor : public IComponentPropertyEditor
{
    Q_OBJECT
public:
    DirectionalLightComponentPropertyEditor();

    void initialize() override;
    void edit(void* component) override;

    void GetLuminousFlux();
    void GetColor();

private:
    QLabel* m_luminousFluxLabel;
    ComboLabelText* m_luminousFlux;

    QLabel* m_colorLabel;
    ComboLabelText* m_colorR;
    ComboLabelText* m_colorG;
    ComboLabelText* m_colorB;

    QValidator* m_validator;

    DirectionalLightComponent* m_component;

public slots:
    void SetLuminousFlux();
    void SetColor();

    void remove() override;
};

#endif // DIRECTIONALLIGHTCOMPONENTPROPERTYEDITOR_H
