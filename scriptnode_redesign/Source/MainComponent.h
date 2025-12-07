#pragma once

#include <JuceHeader.h>

#include "ui/include.h"

namespace scriptnode {
using namespace hise;
using namespace juce;



/** Done:

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
- fix cable node dragging into other containers OK
- fix double connections after duplicating nodes OK
- add free floating comments OK
- fix dragging containers / big nodes into smaller containers OK
- cleanup groups when deleting nodes OK
- remove comment when deleting node OK
- move modulation output 10px down for straight connection line OK

- fix hitzones with new signal paths OK
- fix vertical signal paths (offset S shape for multiple channels) OK
- make signal paths draggable with offset (add CableOffset to Node) OK

- add collapse function & breadcrumb stuff OK

- fix send / receive node stuff OK

- fix connections in locked container OK
- add show node body on right click when folded OK
- add show map on right click OK

TODO:

- make snap markers & auto align to cables & other nodes OK
- add grid for rasterized movements OK
- remove screenshots from database OK
- implement LOD system for better rendering with big patches OK
- fix selection with folded containers OK
- add parameter popup with sliders ("P") OK
- add C++ project nodes
- clone nodes


ROADMAP:

- extra components OK
- clone nodes
- containers with fixed parameters (not draggable, no add possible)
- external data
- add visualisation for modulated parameters (use a query lambda)

BUGS:

- fix alignment & distribution with folded / locked containers
- fix cables being displayed for folded containers
- fix reset mixing up process node positions despite lock
- fix crash when dragging parameter cables
- fix descriptions not appearing anymore
- fix cable nodes not appearing
- fix HideCable being reset when going into locked container
- fix autocomplete appearing within visible area
- update start / end pin connections at LOD change
- fix combobox / toggle button appearing at locked targets

FEATURES:

- add icons for autocomplete popup: polyphony, midi, complex data, unscaled, mod output
- add parameter management tools (add / remove, rename / range / grouping)
- add double click on connection to goto source / target
- add context menu with actions
- add dynamic process signal flag with path stuff for extra_mods (all nodes that have a ProcessSignal / AddToSignal parameter)
- add view undomanager with back buttons

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
                       public scriptnode::DspNetworkComponent::Parent
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

    PooledUIUpdater updater;

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



	scriptnode::NodeDatabase db;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
