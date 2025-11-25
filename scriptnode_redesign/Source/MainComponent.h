#pragma once

#include <JuceHeader.h>

#include "ui/include.h"

namespace scriptnode {
using namespace hise;
using namespace juce;



/** TODO:

- add comments OK
- add floating parameters (break out & move around)
- use a few dummy images OK
- add dragging OK
- optimize space function OK
- add proper delete / reassign OK
- add create popup OK


- refactor: make scriptnode lib objects OK
- add cable curve functions OK
- fix cable node detection (use database) OK
- make database a shared object OK
- fix IsAutomated when creating a cable node with Value OK
- make offline container not part of signal path OK

- add collapse function & breadcrumb stuff 
- fix cable node dragging into other containers
- remove screenshots from database

- fix double connections after duplicating nodes
- fix selection with folded containers
- fix alignment & distribution with folded containers
- add free floating comments
- fix dragging containers / big nodes into smaller containers
- cleanup groups when deleting nodes
- fix send / receive node stuff
- move modulation output 10px down for straight connection line
- 


*/


}



//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::Component,
                       public PathFactory,
                       public valuetree::AnyListener,
	                   public TextEditorWithAutocompleteComponent::Parent
{
    static constexpr int MenuHeight = 24;

public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void anythingChanged(CallbackType cb) override
    {
        auto newContent = currentTree.createXml()->createDocument("");
        codeDoc.replaceAllContent(newContent);
    }

	StringArray getAutocompleteItems(const Identifier& id) override
	{
        auto createSignalNodes = viewport.getContent<scriptnode::DspNetworkComponent>()->createSignalNodes;

        auto list = db.getNodeIds(createSignalNodes);

        if(id.isValid())
        {
            auto prefix = id.toString();

            StringArray matches;
            matches.ensureStorageAllocated(list.size());

            for(const auto& l: list)
            {
                if(l.startsWith(prefix))
                    matches.add(l.fromFirstOccurrenceOf(prefix, false, false));
            }

            return matches;
        }

        return list;
	}

private:

    scriptnode::NodeDatabase db;

    //==============================================================================
    // Your private member variables go here...

    Path createPath(const String& url) const override;
    
    hise::ZoomableViewport viewport;

    juce::CodeDocument codeDoc;
    mcl::TextDocument doc;
    mcl::TextEditor viewer;

    ValueTree currentTree;

    hise::HiseShapeButton openButton;

	HiseShapeButton buttonDeselectAll;
	HiseShapeButton buttonCut;
	HiseShapeButton buttonCopy;
	HiseShapeButton buttonPaste;
	HiseShapeButton buttonDuplicate;
	HiseShapeButton buttonToggleVertical;
	HiseShapeButton buttonAddComment;
	HiseShapeButton buttonUndo;
	HiseShapeButton buttonRedo;
	HiseShapeButton buttonAutoLayout;
	HiseShapeButton buttonAlignTop;
	HiseShapeButton buttonAlignLeft;
    HiseShapeButton buttonSetColour;
	HiseShapeButton buttonDistributeHorizontally;
	HiseShapeButton buttonDistributeVertically;



	

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
