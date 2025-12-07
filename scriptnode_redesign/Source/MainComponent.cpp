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
  buttonSetColour("SetColour", nullptr, *this),
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

	auto projects = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("HISE/projects.xml");

	if(auto xml = XmlDocument::parse(projects))
	{
		auto currentProject = xml->getStringAttribute("current");

		if(File::isAbsolutePath(currentProject))
		{
			auto cf = File(currentProject).getChildFile("DspNetworks/ThirdParty");

			if(cf.isDirectory())
				db.setProjectDataFolder(cf);
		}
	}

	db.processValueTrees([](ValueTree v)
	{
		using namespace scriptnode;

		ScopedPointer<NodeComponent> nc;

		if (scriptnode::Helpers::isContainerNode(v))
			nc = new ContainerComponent(nullptr, v, nullptr);
		else if (scriptnode::Helpers::isProcessNode(v))
			nc = new ProcessNodeComponent(nullptr, v, nullptr);
		else
			nc = new NoProcessNodeComponent(nullptr, v, nullptr);

		v.setProperty(UIPropertyIds::width, nc->getWidth(), nullptr);
		v.setProperty(UIPropertyIds::height, nc->getHeight(), nullptr);

		nc = nullptr;
	});



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
	attach(buttonSetColour, scriptnode::DspNetworkComponent::Action::SetColour);
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

	viewport.setMaxZoomFactor(1.5);
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
				
				auto rootContainer = currentTree.getChild(0);

                viewport.setNewContent(new scriptnode::DspNetworkComponent(&updater, viewport, currentTree, rootContainer), nullptr);
				
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
	placeButton(buttonSetColour);
    top.removeFromLeft(20);
	placeButton(buttonUndo);
	placeButton(buttonRedo);
    top.removeFromLeft(20);
	placeButton(buttonAutoLayout);
	placeButton(buttonAlignTop);
	placeButton(buttonAlignLeft);
	
	placeButton(buttonDistributeHorizontally);
	placeButton(buttonDistributeVertically);

	viewer.setBounds(b.removeFromLeft(jmin(800, getWidth() / 4)));

    viewport.setBounds(b);
}

namespace ScriptnodeIcons
{
	static const unsigned char colourIcon[] = { 110,109,115,104,89,64,143,194,9,65,98,248,83,35,63,63,53,34,65,219,249,126,191,203,161,93,65,186,73,44,63,90,100,135,65,98,66,96,21,64,254,212,159,65,180,200,218,64,106,188,167,65,37,6,27,65,211,77,151,65,98,27,47,7,65,231,251,138,65,186,73,248,64,84,
	227,117,65,209,34,251,64,14,45,86,65,98,203,161,249,64,84,227,85,65,197,32,248,64,129,149,85,65,190,159,246,64,174,71,85,65,98,125,63,181,64,133,235,71,65,49,8,132,64,209,34,43,65,115,104,89,64,143,194,9,65,99,109,229,208,144,65,63,53,6,65,98,182,243,
	143,65,221,36,12,65,254,212,142,65,74,12,18,65,152,110,141,65,35,219,23,65,98,133,235,134,65,47,221,50,65,197,32,118,65,55,137,71,65,117,147,90,65,233,38,83,65,98,201,118,94,65,98,16,108,65,180,200,90,65,172,28,131,65,170,241,76,65,209,34,143,65,98,217,
	206,73,65,59,223,145,65,63,53,70,65,139,108,148,65,113,61,66,65,217,206,150,65,98,166,155,102,65,102,102,162,65,115,104,141,65,106,188,162,65,244,253,157,65,174,71,147,65,98,231,251,173,65,78,98,132,65,123,20,178,65,152,110,84,65,184,30,167,65,4,86,46,
	65,98,37,6,162,65,215,163,28,65,123,20,154,65,240,167,14,65,229,208,144,65,63,53,6,65,99,109,49,8,66,65,23,217,90,65,98,102,102,52,65,119,190,93,65,29,90,38,65,250,126,94,65,117,147,24,65,229,208,92,65,98,252,169,23,65,150,67,107,65,172,28,26,65,123,
	20,122,65,106,188,32,65,209,34,132,65,98,246,40,36,65,193,202,135,65,141,151,40,65,184,30,139,65,217,206,45,65,160,26,142,65,98,35,219,59,65,35,219,132,65,12,2,67,65,147,24,114,65,49,8,66,65,23,217,90,65,99,109,137,65,46,65,131,192,36,65,98,193,202,39,
	65,233,38,45,65,43,135,34,65,18,131,54,65,180,200,30,65,127,106,64,65,98,47,221,40,65,143,194,65,65,184,30,51,65,45,178,65,65,184,30,61,65,197,32,64,65,98,98,16,60,65,12,2,61,65,254,212,58,65,133,235,57,65,115,104,57,65,96,229,54,65,98,78,98,54,65,127,
	106,48,65,190,159,50,65,4,86,42,65,137,65,46,65,131,192,36,65,99,109,8,172,131,65,215,163,252,64,98,2,43,129,65,160,26,251,64,125,63,125,65,4,86,250,64,197,32,120,65,102,102,250,64,98,82,184,98,65,100,59,251,64,141,151,78,65,113,61,6,65,78,98,62,65,84,
	227,19,65,98,139,108,71,65,150,67,31,65,106,188,78,65,98,16,44,65,10,215,83,65,20,174,57,65,98,219,249,92,65,193,202,53,65,6,129,101,65,78,98,48,65,61,10,109,65,41,92,41,65,98,55,137,121,65,70,182,29,65,100,59,129,65,215,163,14,65,8,172,131,65,215,163,
	252,64,99,109,229,208,24,65,201,118,16,65,98,211,77,8,65,170,241,4,65,68,139,232,64,68,139,252,64,219,249,190,64,254,212,252,64,98,72,225,178,64,86,14,253,64,219,249,166,64,141,151,254,64,188,116,155,64,240,167,0,65,98,102,102,158,64,125,63,5,65,221,
	36,162,64,193,202,9,65,82,184,166,64,162,69,14,65,98,180,200,186,64,10,215,33,65,174,71,221,64,4,86,48,65,33,176,2,65,205,204,56,65,98,178,157,3,65,168,198,53,65,33,176,4,65,205,204,50,65,109,231,5,65,10,215,47,65,98,117,147,10,65,152,110,36,65,12,2,
	17,65,59,223,25,65,229,208,24,65,201,118,16,65,99,109,254,212,133,65,217,206,199,64,98,147,24,134,65,70,182,167,64,201,118,132,65,139,108,135,64,156,196,128,65,27,47,85,64,98,12,2,113,65,39,49,168,63,10,215,77,65,244,253,84,188,27,47,41,65,0,0,0,0,98,
	82,184,234,64,37,6,129,61,4,86,146,64,80,141,71,64,121,233,146,64,119,190,203,64,98,51,51,163,64,18,131,200,64,59,223,179,64,23,217,198,64,106,188,196,64,219,249,198,64,98,133,235,249,64,158,239,199,64,84,227,21,65,86,14,221,64,14,45,42,65,43,135,254,
	64,98,43,135,64,65,182,243,217,64,209,34,93,65,246,40,196,64,12,2,123,65,18,131,196,64,98,236,81,128,65,106,188,196,64,160,26,131,65,35,219,197,64,254,212,133,65,217,206,199,64,99,101,0,0 };

	static const unsigned char swapOrientationIcon[] = { 110,109,176,186,20,68,113,237,218,67,98,127,106,24,68,113,237,218,67,197,104,27,68,252,233,224,67,197,104,27,68,154,73,232,67,108,197,104,27,68,27,47,8,68,98,197,104,27,68,233,222,11,68,127,106,24,68,31,221,14,68,176,186,20,68,47,221,14,68,108,248,51,
18,67,47,221,14,68,98,188,116,3,67,47,221,14,68,76,247,238,66,233,222,11,68,76,247,238,66,27,47,8,68,108,76,247,238,66,154,73,232,67,98,76,247,238,66,252,233,224,67,188,116,3,67,113,237,218,67,248,51,18,67,113,237,218,67,108,176,186,20,68,113,237,218,
67,99,109,92,207,237,66,0,0,0,0,98,180,8,254,66,0,0,0,0,219,153,5,67,82,184,210,64,219,153,5,67,209,34,107,65,108,219,153,5,67,248,243,195,67,108,236,145,223,66,248,243,195,67,98,227,165,184,66,25,244,195,67,205,12,153,66,94,218,203,67,205,12,153,66,
96,149,213,67,98,205,12,153,66,96,149,213,67,184,94,153,66,254,68,255,67,55,137,153,66,141,167,6,68,98,223,143,153,66,203,193,7,68,129,149,153,66,205,132,8,68,154,153,153,66,4,206,8,68,108,209,34,107,65,45,202,8,68,98,82,184,210,64,45,202,8,68,0,0,0,
0,188,36,7,68,0,0,0,0,162,29,5,68,108,0,0,0,0,209,34,107,65,98,0,0,0,0,82,184,210,64,82,184,210,64,0,0,0,0,209,34,107,65,0,0,0,0,108,92,207,237,66,0,0,0,0,99,109,96,133,221,67,92,111,91,67,108,170,201,6,68,233,166,246,66,108,170,201,6,68,111,18,192,67,
108,0,16,139,67,14,45,192,67,108,18,163,177,67,252,153,153,67,108,109,135,66,67,63,117,18,67,108,4,38,141,67,143,194,106,66,108,96,133,221,67,92,111,91,67,99,101,0,0 };

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
	LOAD_PATH_IF_URL("ToggleVertical", ScriptnodeIcons::swapOrientationIcon);
	LOAD_EPATH_IF_URL("AddComment", ColumnIcons::commentIcon);
	LOAD_EPATH_IF_URL("Undo", EditorIcons::undoIcon);
	LOAD_EPATH_IF_URL("Redo", EditorIcons::redoIcon);
	LOAD_EPATH_IF_URL("AutoLayout", EditorIcons::connectIcon);
	LOAD_EPATH_IF_URL("AlignTop", EditorIcons::horizontalAlign);
	LOAD_EPATH_IF_URL("AlignLeft", EditorIcons::verticalAlign);
	LOAD_PATH_IF_URL("SetColour", ScriptnodeIcons::colourIcon);
	LOAD_EPATH_IF_URL("DistributeHorizontally", ColumnIcons::horizontalDistribute);
	LOAD_EPATH_IF_URL("DistributeVertically", ColumnIcons::verticalDistribute);

	return p;
}

namespace scriptnode {
using namespace hise;
using namespace juce;




}


