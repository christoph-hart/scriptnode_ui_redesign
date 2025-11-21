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


- add create popup
- fix dragging into other containers
- refactor: make scriptnode lib objects
- make offline container not part of signal path




*/


}



//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::Component,
                       public PathFactory,
                       public valuetree::AnyListener
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

private:

    

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
	HiseShapeButton buttonDistributeHorizontally;
	HiseShapeButton buttonDistributeVertically;



	

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
