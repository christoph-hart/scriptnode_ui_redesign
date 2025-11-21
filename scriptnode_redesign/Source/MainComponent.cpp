#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent():
  openButton("open", nullptr, *this),
  buttonDeselectAll("DeselectAll", nullptr, *this),
  buttonCut("Cut", nullptr, *this),
  buttonCopy("Copy", nullptr, *this),
  buttonPaste("Paste", nullptr, *this),
  buttonDuplicate("Duplicate", nullptr, *this),
  buttonToggleVertical("ToggleVertical", nullptr, *this),
  buttonAddComment("AddComment", nullptr, *this),
  buttonUndo("Undo", nullptr, *this),
  buttonRedo("Redo", nullptr, *this),
  buttonAutoLayout("AutoLayout", nullptr, *this),
  buttonAlignTop("AlignTop", nullptr, *this),
  buttonAlignLeft("AlignLeft", nullptr, *this),
  buttonDistributeHorizontally("DistributeHorizontally", nullptr, *this),
  buttonDistributeVertically("DistributeVertically", nullptr, *this),
  viewport(new Component()),
  doc(codeDoc),
  viewer(doc)
{
	auto attach = [&](HiseShapeButton& b, scriptnode::DspNetworkComponent::Action a)
	{
		addAndMakeVisible(b);

		b.onClick = [a, this]()
		{
			if (auto dn = viewport.getContent<scriptnode::DspNetworkComponent>())
			{
				dn->performAction(a);
			}
		};
	};

    attach(buttonDeselectAll, scriptnode::DspNetworkComponent::Action::DeselectAll);
    attach(buttonCut, scriptnode::DspNetworkComponent::Action::Cut);
    attach(buttonCopy, scriptnode::DspNetworkComponent::Action::Copy);
    attach(buttonPaste, scriptnode::DspNetworkComponent::Action::Paste);
    attach(buttonDuplicate, scriptnode::DspNetworkComponent::Action::Duplicate);
    attach(buttonToggleVertical, scriptnode::DspNetworkComponent::Action::ToggleVertical);
    attach(buttonAddComment, scriptnode::DspNetworkComponent::Action::AddComment);
    attach(buttonUndo, scriptnode::DspNetworkComponent::Action::Undo);
    attach(buttonRedo, scriptnode::DspNetworkComponent::Action::Redo);
    attach(buttonAutoLayout, scriptnode::DspNetworkComponent::Action::AutoLayout);
    attach(buttonAlignTop, scriptnode::DspNetworkComponent::Action::AlignTop);
    attach(buttonAlignLeft, scriptnode::DspNetworkComponent::Action::AlignLeft);
    attach(buttonDistributeHorizontally, scriptnode::DspNetworkComponent::Action::DistributeHorizontally);
    attach(buttonDistributeVertically, scriptnode::DspNetworkComponent::Action::DistributeVertically);

	viewer.setReadOnly(true);
	codeDoc.setDisableUndo(true);
	addAndMakeVisible(viewer);

	viewer.setLanguageManager(new mcl::XmlLanguageManager());

    addAndMakeVisible(viewport);
    viewport.contentFunction = [](Component* ) {};
    viewport.setEnableWASD([](Component*){
        return true;
    });

    viewport.setScrollOnDragEnabled(true);

	viewport.setMouseWheelScrollEnabled(true);

    setSize (3000, 900);

    addAndMakeVisible(openButton);

    openButton.onClick = [this]()
    {
        FileChooser fc("Open network XML", File(), "*.xml", true);

        if(fc.browseForFileToOpen())
        {
            if(auto xml = XmlDocument::parse(fc.getResult()))
            {
                currentTree = ValueTree::fromXml(*xml);

				setRootValueTree(currentTree);
				setMillisecondsBetweenUpdate(1000);
				
                viewport.setNewContent(new scriptnode::DspNetworkComponent(currentTree), nullptr);
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

    auto placeButton = [&](HiseShapeButton& b)
    {
        b.setBounds(top.removeFromLeft(MenuHeight).reduced(2));
    };

    
    placeButton(openButton);
    top.removeFromLeft(20);
	placeButton(buttonDeselectAll);
	placeButton(buttonCut);
	placeButton(buttonCopy);
	placeButton(buttonPaste);
	placeButton(buttonDuplicate);
    top.removeFromLeft(20);
	placeButton(buttonToggleVertical);
	placeButton(buttonAddComment);
    top.removeFromLeft(20);
	placeButton(buttonUndo);
	placeButton(buttonRedo);
    top.removeFromLeft(20);
	placeButton(buttonAutoLayout);
	placeButton(buttonAlignTop);
	placeButton(buttonAlignLeft);
	placeButton(buttonDistributeHorizontally);
	placeButton(buttonDistributeVertically);

	viewer.setBounds(b.removeFromLeft(800));

    viewport.setBounds(b);
}

juce::Path MainComponent::createPath(const String& url) const
{
	Path p;

	LOAD_EPATH_IF_URL("open", EditorIcons::openFile);

	LOAD_EPATH_IF_URL("DeselectAll", EditorIcons::cancelIcon);
	LOAD_EPATH_IF_URL("Cut", SampleMapIcons::cutSamples);
	LOAD_EPATH_IF_URL("Copy", SampleMapIcons::copySamples);
	LOAD_EPATH_IF_URL("Paste", SampleMapIcons::pasteSamples);
	LOAD_EPATH_IF_URL("Duplicate", SampleMapIcons::duplicateSamples);
	LOAD_EPATH_IF_URL("ToggleVertical", EditorIcons::swapIcon);
	LOAD_EPATH_IF_URL("AddComment", ColumnIcons::commentIcon);
	LOAD_EPATH_IF_URL("Undo", EditorIcons::undoIcon);
	LOAD_EPATH_IF_URL("Redo", EditorIcons::redoIcon);
	LOAD_EPATH_IF_URL("AutoLayout", EditorIcons::connectIcon);
	LOAD_EPATH_IF_URL("AlignTop", EditorIcons::horizontalAlign);
	LOAD_EPATH_IF_URL("AlignLeft", EditorIcons::verticalAlign);
	LOAD_EPATH_IF_URL("DistributeHorizontally", ColumnIcons::horizontalDistribute);
	LOAD_EPATH_IF_URL("DistributeVertically", ColumnIcons::verticalDistribute);

	return p;
}

namespace scriptnode {
using namespace hise;
using namespace juce;




}


