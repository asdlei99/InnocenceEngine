#include "innoworldexplorer.h"

#include "../../engine/system/ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

InnoWorldExplorer::InnoWorldExplorer(QWidget* parent) : QTreeWidget(parent)
{
}

void InnoWorldExplorer::buildTree()
{
    m_rootItem = new QTreeWidgetItem(this);
    m_rootItem->setText(0, "Entities");
    this->addTopLevelItem(m_rootItem);

    auto l_entityNameMap = g_pCoreSystem->getGameSystem()->getEntityNameMap();
    auto l_entityChildrenComponentsMetadataMap = g_pCoreSystem->getGameSystem()->getEntityChildrenComponentsMetadataMap();

    for (auto i : l_entityNameMap)
    {
        QTreeWidgetItem* l_entityItem = new QTreeWidgetItem();
        l_entityItem->setText(0, i.second.c_str());
        l_entityItem->setData(0, Qt::UserRole, QVariant(-1));

        addChild(m_rootItem, l_entityItem);

        auto result = l_entityChildrenComponentsMetadataMap.find(i.first);
        if (result != l_entityChildrenComponentsMetadataMap.end())
        {
            auto& l_componentMetadataMap = result->second;

            for (auto& j : l_componentMetadataMap)
            {
                auto& l_componentMetapair = j.second;
                QTreeWidgetItem* l_componentItem = new QTreeWidgetItem();
                l_componentItem->setText(0, l_componentMetapair.second.c_str());
                l_componentItem->setData(0, Qt::UserRole, QVariant((int)l_componentMetapair.first));
                l_componentItem->setData(1, Qt::UserRole, QVariant::fromValue(j.first));
                addChild(l_entityItem, l_componentItem);
            }
        }
    }
}

void InnoWorldExplorer::initialize(InnoPropertyEditor* propertyEditor)
{
    m_propertyEditor = propertyEditor;

    buildTree();

    f_sceneLoadingFinishCallback
            = [&]()
    {
        clear();
        buildTree();
    };

    g_pCoreSystem->getFileSystem()->addSceneLoadingFinishCallback(&f_sceneLoadingFinishCallback);
}

void InnoWorldExplorer::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QTreeWidget::selectionChanged(selected, deselected);
    QList<QTreeWidgetItem*> selectedItems = this->selectedItems();
    QTreeWidgetItem* item;
    if (selectedItems.count() != 0)
    {
        item = selectedItems[0];
        if(item != m_rootItem)
        {
            if (m_propertyEditor)
            {
                auto l_componentType = item->data(0, Qt::UserRole).toInt();
                if(l_componentType != -1)
                {
                    auto l_componentPtr = item->data(1, Qt::UserRole).value<void*>();
                    m_propertyEditor->editComponent(l_componentType, l_componentPtr);
                }
                else
                {
                    m_propertyEditor->remove();
                }

            }
        }
    }
}

void InnoWorldExplorer::addChild(QTreeWidgetItem *parent, QTreeWidgetItem *child)
{
    parent->addChild(child);
}
