#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent():
  openButton("open", nullptr, *this),
  viewport(new Component())
{
    addAndMakeVisible(viewport);
    viewport.contentFunction = [](Component* ) {};

    setSize (600, 400);

    addAndMakeVisible(openButton);

    openButton.onClick = [this]()
    {
        FileChooser fc("Open network XML", File(), "*.xml", true);

        if(fc.browseForFileToOpen())
        {
            if(auto xml = XmlDocument::parse(fc.getResult()))
            {
                auto v = ValueTree::fromXml(*xml);

                viewport.setNewContent(new scriptnode::DspNetworkComponent(v), nullptr);
                resized();
            }
        }
    };
}

MainComponent::~MainComponent()
{
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll(Colour(0xFF222222));
}

void MainComponent::resized()
{
    auto b = getLocalBounds();

    auto top = b.removeFromTop(MenuHeight);

    openButton.setBounds(top.removeFromLeft(MenuHeight).reduced(2));
    
    viewport.setBounds(b);
}

juce::Path MainComponent::createPath(const String& url) const
{
	Path p;

	LOAD_EPATH_IF_URL("open", EditorIcons::openFile);

	return p;
}

namespace scriptnode {
using namespace hise;
using namespace juce;

void NodeComponent::HeaderComponent::mouseUp(const MouseEvent& e)
{
    auto bounds = parent.getBoundsInParent();
    Helpers::updateBounds(parent.getValueTree(), bounds, parent.um);

	auto root = findParentComponentOfClass<DspNetworkComponent>();
    Helpers::fixOverlap(root->data.getChildWithName(PropertyIds::Node), &root->um);
    
    Component::callRecursive<ContainerComponent>(root, [](ContainerComponent* c)
    {
        c->rebuildCables();
        return false;
    });
}

}


