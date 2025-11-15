#pragma once

#include <JuceHeader.h>

namespace scriptnode {
using namespace hise;
using namespace juce;

namespace UIPropertyIds
{
#define DECLARE_ID(x) static const Identifier x(#x);
DECLARE_ID(x);
DECLARE_ID(y);
DECLARE_ID(width);
DECLARE_ID(height);
#undef DECLARE_ID
}

struct Helpers
{
    struct CableSetup
    {
        enum class ContainerType
        {
            Serial,
            Split,
            Multi,
            Branch
        };

        struct Connection
        {
            int sourceIndex;
            int targetIndex;
            bool active;
        };

		struct Pins
		{
			Point<float> containerStart;
            Point<float> containerEnd;

			std::vector<std::pair<Point<float>, Point<float>>> childPositions;
		};

		using MultiChannelConnection = std::vector<Connection>;

        CableSetup(const ValueTree& v, int numChannels_):
          numChannels(numChannels_),
          numNodes(v.getChildWithName(PropertyIds::Nodes).getNumChildren())
        {
            auto p = v[PropertyIds::FactoryPath].toString();

			if (p == "container.split")
            {
                type = ContainerType::Split;

                MultiChannelConnection mc;

                for(int i = 0; i < numChannels; i++)
                    mc.push_back({i, i, true});

                for(int i = 0; i < numNodes; i++)
                    connections.push_back(mc);
            }
			else if (p == "container.multi")
            {
                type = ContainerType::Multi;

                for(int i = 0; i < numNodes; i++)
                {
                    MultiChannelConnection mc;
                    mc.push_back({i, i, true});
                    connections.push_back(mc);
                }
            }
            else if(p == "container.branch")
            {
                type = ContainerType::Branch;
                
                // use parameter Value to determine active branch
                jassertfalse;
            }
			else
            {
                type = ContainerType::Serial;

				MultiChannelConnection mc;

				for (int i = 0; i < numChannels; i++)
					mc.push_back({ i, i, true });

                connections.push_back(mc);
            }
        }
        
        static void drawCable(Graphics& g, Point<float> p1, Point<float> p2)
        {
            g.setColour(Colours::white);
            g.drawLine({p1, p2});
        }

        void draw(Graphics& g, const Pins& pinPositions)
        {
            const auto delta = (float)(SignalHeight) / (float)numChannels;
            auto xOffset = 10.0f;

            if(type == ContainerType::Serial)
            {
                for(int i = 0; i < pinPositions.childPositions.size(); i++)
                {
                    float offset = delta * 0.5f;

                    for(int c = 0; c < numChannels; c++)
                    {
						auto p1 = i == 0 ? pinPositions.containerStart.translated(xOffset, 0.0f) : 
                                           pinPositions.childPositions[i - 1].second.translated(-xOffset, 0.0f);

						auto p2 = pinPositions.childPositions[i].first;
						drawCable(g, p1.translated(0, offset), p2.translated(xOffset, offset));
                        offset += delta;
                    }
                }

				float offset = delta * 0.5f;

				for (int c = 0; c < numChannels; c++)
				{
					auto p1 = pinPositions.childPositions.back().second;
					auto p2 = pinPositions.containerEnd;
					drawCable(g, p1.translated(-xOffset, offset), p2.translated(-xOffset, offset));
					offset += delta;
				}
            }
            else
            {
				for (int i = 0; i < connections.size(); i++)
				{
                    std::vector<Connection> con = connections[i];

                    for(auto cc: con)
                    {
                        float offset = delta * 0.5f + (float)cc.sourceIndex * delta;
                        auto p1 = pinPositions.containerStart.translated(xOffset, offset);
                        auto p2 = pinPositions.childPositions[i].first.translated(xOffset, offset);
                        auto p3 = pinPositions.childPositions[i].second.translated(-xOffset, offset);
                        auto p4 = pinPositions.containerEnd.translated(-xOffset, offset);
                        drawCable(g, p1, p2);
                        drawCable(g, p3, p4);
                    }
				}
            }
            
        }

        ContainerType type;
        const int numChannels;
        const int numNodes;
        std::vector<MultiChannelConnection> connections;
    };

	static constexpr int HeaderHeight = 24;
	static constexpr int SignalHeight = 40;
    static constexpr int NodeMargin = 50;

    static bool isProcessNode(const ValueTree& v)
    {
        jassert(v.getType() == PropertyIds::Node);
        return v[PropertyIds::FactoryPath].toString().upToFirstOccurrenceOf(".", false, false) != "control";
    }

    static bool isContainerNode(const ValueTree& v)
    {
        jassert(v.getType() == PropertyIds::Node);
        return v[PropertyIds::FactoryPath].toString().upToFirstOccurrenceOf(".", false, false) == "container";
    }

    static bool shouldBeVertical(const ValueTree& container, const ValueTree& node)
    {
        jassert(container.getType() == PropertyIds::Node);
        jassert(node.getType() == PropertyIds::Node);

        if(container[PropertyIds::IsVertical])
            return true;

        auto p = container[PropertyIds::FactoryPath].toString();

        static const StringArray verticalContainers = {
            "container.split",
            "container.multi"
        };

        if(verticalContainers.contains(p))
            return true;

        return !isProcessNode(node);
    }

    static void setMinPosition(Rectangle<int>& b, Point<int> minOffset)
    {
        b.setPosition(jmax<int>(minOffset.getX(), b.getX()), jmax<int>(minOffset.getY(), b.getY()));
    }

    static void fixOverlapRecursive(ValueTree& node, UndoManager* um)
    {
        jassert(node.getType() == PropertyIds::Node);
		auto bounds = getBounds(node);

        auto nodes = node.getChildWithName(PropertyIds::Nodes);

        auto thisType = node[PropertyIds::FactoryPath].toString();

        auto minX = 30;

        if(node[PropertyIds::ShowParameters])
            minX += 100;

        auto minY = 30 + HeaderHeight;

        if(isProcessNode(node) || isContainerNode(node))
            minY += SignalHeight;

		RectangleList<int> childBounds;

		for (auto childNode : nodes)
		{
            fixOverlapRecursive(childNode, um);

			auto cb = getBounds(childNode);

            setMinPosition(cb, { minX, minY });

			if (childBounds.intersectsRectangle(cb))
			{
				auto fullBounds = childBounds.getBounds();

				if (shouldBeVertical(node, childNode))
					cb = cb.withY(fullBounds.getBottom() + NodeMargin);
				else
					cb = cb.withX(fullBounds.getRight() + NodeMargin);
			}

			childNode.setProperty(UIPropertyIds::x, cb.getX(), um);
			childNode.setProperty(UIPropertyIds::y, cb.getY(), um);

			childBounds.addWithoutMerging(cb);
		}

		auto fullBounds = childBounds.getBounds();

		auto currentWidth = (int)node[UIPropertyIds::width];
		auto currentHeight = (int)node[UIPropertyIds::height];

		updateBounds(node, {
            jmax(bounds.getX(), minX),
            jmax(bounds.getY(), minY),
            jmax(currentWidth, fullBounds.getWidth() + NodeMargin, bounds.getWidth()),
            jmax(currentHeight, fullBounds.getHeight() + NodeMargin, bounds.getHeight()),
            }, um);
    }

    static void updateBounds(ValueTree& v, Rectangle<int> newBounds, UndoManager* um)
    {
        jassert(v.getType() == PropertyIds::Node);

        auto prevBounds = getBounds(v);

        if(prevBounds != newBounds)
        {
            String msg;
            msg << getHeaderTitle(v);
            msg << prevBounds.toString() << " -> " << newBounds.toString();
            DBG(msg);
        }

		v.setProperty(UIPropertyIds::x, newBounds.getX(), um);
		v.setProperty(UIPropertyIds::y, newBounds.getY(), um);
		v.setProperty(UIPropertyIds::width, newBounds.getWidth(), um);
		v.setProperty(UIPropertyIds::height, newBounds.getHeight(), um);
    }

    static void fixOverlap(ValueTree& v, UndoManager* um)
    {
        fixOverlapRecursive(v, um);
    }

    static Colour getNodeColour(const ValueTree& v)
    {
        jassert(v.getType() == PropertyIds::Node);

        auto c = (int64)v[PropertyIds::NodeColour];

        if(c != 0)
            return Colour((uint32)c);

        if(isContainerNode(v))
            return Colour(0xFF555555);

        return Colour(0xFF777777);
    }
    
    static String getHeaderTitle(const ValueTree& v)
    {
        jassert(v.getType() == PropertyIds::Node);

        auto text = v[PropertyIds::Name].toString();

        if(text.isEmpty())
            text = v[PropertyIds::ID].toString();

        text << " (" << v[PropertyIds::FactoryPath].toString() << ")";

        return text;
    }

    static Point<int> getPosition(const ValueTree& v)
    {
        auto x = (int)v[UIPropertyIds::x];
        auto y = (int)v[UIPropertyIds::y];

        if(v.getParent().getType() == PropertyIds::Network)
            return { 0, 0 };

        return { x, y };
    }

    static Rectangle<int> getBounds(const ValueTree& v)
    {
        auto p = getPosition(v);
        auto w = (int)v[UIPropertyIds::width];
        auto h = (int)v[UIPropertyIds::height];

        if(w == 0)
            w = 250;
        if(h == 0)
            h = 100;

        if(v[PropertyIds::ShowParameters])
            w += 100;

        if(v[PropertyIds::Folded])
            h = 50;

        return Rectangle<int>(p, p.translated(w, h));
    }

    static constexpr valuetree::AsyncMode UIMode = valuetree::AsyncMode::Synchronously;
};

struct NodeComponent: public Component,
                      public PathFactory
{
    struct HeaderComponent: public Component
    {
        HeaderComponent(NodeComponent& parent_):
          parent(parent_),
          powerButton("bypass", nullptr, parent_),
          dragger()
        {};

        void resized() override
        {
            auto b = getLocalBounds();
            powerButton.setBounds(b.removeFromLeft(getHeight()).reduced(2));
        }

        void mouseDoubleClick(const MouseEvent& event) override
        {
            parent.toggle(PropertyIds::Folded);
            Helpers::fixOverlap(parent.getValueTree(), parent.um);
        }

        void mouseDown(const MouseEvent& e) override
        {
            dragger.startDraggingComponent(&parent, e);
        }

        void mouseDrag(const MouseEvent& e) override
        {
            dragger.dragComponent(&parent, e, nullptr);
            parent.findParentComponentOfClass<Component>()->repaint();
        }

        void mouseUp(const MouseEvent& e) override;

        void paint(Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
			g.setColour(Helpers::getNodeColour(parent.getValueTree()));
			g.fillRect(b);
			g.setColour(Colours::white);
			g.drawText(Helpers::getHeaderTitle(parent.getValueTree()), b, Justification::centred);
        }

        NodeComponent& parent;
        HiseShapeButton powerButton;
        juce::ComponentDragger dragger;
    };

    struct SignalComponent: public Component
    {
        SignalComponent(NodeComponent& parent_):
          parent(parent_)
        {};

        void paint(Graphics& g) override
        {
            g.fillAll(Colour(0x55222222));
        }

        NodeComponent& parent;
    };

    void toggle(const Identifier& id)
    {
        auto v = (bool)data[id];
        data.setProperty(id, !v, um);
    }

    NodeComponent(const ValueTree& v, UndoManager* um_, bool useSignal):
      data(v),
      um(um_),
      header(*this)
    {
        addAndMakeVisible(header);

        if(useSignal)
        {
            addAndMakeVisible(signalComponent = new SignalComponent(*this));
        }

        positionListener.setCallback(v, 
            { UIPropertyIds::x, UIPropertyIds::y }, 
            Helpers::UIMode, 
            VT_BIND_PROPERTY_LISTENER(onPositionUpdate));

        setBounds(Helpers::getBounds(v));
    };

    virtual ~NodeComponent() {};

    Path createPath(const String& url) const override
    {
        Path p;

        LOAD_EPATH_IF_URL("bypass", HiBinaryData::ProcessorEditorHeaderIcons::bypassShape);

        return p;
    }

    void onPositionUpdate(const Identifier&, const var&)
    {
        setTopLeftPosition(Helpers::getPosition(data));
    }

    void resized() override
    {
        auto b = getLocalBounds();
        header.setBounds(b.removeFromTop(Helpers::HeaderHeight));

        if(signalComponent != nullptr)
            signalComponent->setBounds(b.removeFromTop(Helpers::SignalHeight));
    }

    ValueTree getValueTree() const { return data; }

    SignalComponent* getSignalComponent() const { return signalComponent.get(); }

protected:

    ScopedPointer<SignalComponent> signalComponent;
    HeaderComponent header;

    valuetree::PropertyListener positionListener;

    ValueTree data;
    UndoManager* um;
};



struct ProcessNodeComponent: public NodeComponent
{
    ProcessNodeComponent(const ValueTree& v, UndoManager* um_):
      NodeComponent(v, um_, true)
    {
		
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xFF333333));
    }
};

struct NoProcessNodeComponent: public NodeComponent
{
	NoProcessNodeComponent(const ValueTree& v, UndoManager* um_) :
		NodeComponent(v, um_, false)
	{
		
	}

    void paint(Graphics& g) override
    {
		g.fillAll(Colour(0xFF333333));
    }
};

struct ContainerComponent : public NodeComponent
{
	ContainerComponent(const ValueTree& v, UndoManager* um_) :
		NodeComponent(v, um_, true),
		resizer(this, nullptr)
	{
		addAndMakeVisible(resizer);

		nodeListener.setCallback(data.getChildWithName(PropertyIds::Nodes),
			Helpers::UIMode,
			VT_BIND_CHILD_LISTENER(onChildAddRemove));

		resizeListener.setCallback(data,
			{ UIPropertyIds::width, UIPropertyIds::height },
			Helpers::UIMode,
			VT_BIND_PROPERTY_LISTENER(onResize));

        resizer.setAlwaysOnTop(true);
	}

    void paint(Graphics& g) override
    {
		g.setColour(Helpers::getNodeColour(data));
        g.drawRect(getLocalBounds(), 1);

        Helpers::CableSetup::Pins pins;
        pins.containerStart = signalComponent->getPosition().toFloat();
        pins.containerEnd = signalComponent->getBoundsInParent().getTopRight().toFloat();

        Helpers::CableSetup cs(getValueTree(), 2);

        for(auto cn: childNodes)
        {
			if (auto sc = cn->getSignalComponent())
			{
                auto sb = getLocalArea(sc, sc->getLocalBounds()).toFloat();
                pins.childPositions.push_back({ sb.getTopLeft(), sb.getTopRight() });
            }
        }

        cs.draw(g, pins);
    }

	void onChildAddRemove(const ValueTree& v, bool wasAdded)
	{
		if (wasAdded)
		{
			NodeComponent* nc;

			if (Helpers::isContainerNode(v))
				nc = new ContainerComponent(v, um);
			else if (Helpers::isProcessNode(v))
				nc = new ProcessNodeComponent(v, um);
			else
				nc = new NoProcessNodeComponent(v, um);

			addAndMakeVisible(nc);
			childNodes.add(nc);
		}
		else
		{
			for (auto c : childNodes)
			{
				if (c->getValueTree() == v)
				{
					childNodes.removeObject(c);
					break;
				}
			}
		}
	}

	void onResize(const Identifier& id, const var& newValue)
	{
		if (id == UIPropertyIds::width)
			setSize((int)newValue, getHeight());
		else
			setSize(getWidth(), (int)newValue);
	}

	void resized() override
	{
        NodeComponent::resized();

		auto b = getLocalBounds();

		data.setProperty(UIPropertyIds::width, getWidth(), um);
		data.setProperty(UIPropertyIds::height, getHeight(), um);

		resizer.setBounds(b.removeFromRight(15).removeFromBottom(15));
	}

	juce::ResizableCornerComponent resizer;
	OwnedArray<NodeComponent> childNodes;

	valuetree::PropertyListener resizeListener;
	valuetree::ChildListener nodeListener;
};

struct DspNetworkComponent: public Component
{
    DspNetworkComponent(const ValueTree& v_):
      data(v_)
    {
        auto rootContainer = data.getChild(0);
        jassert(rootContainer.getType() == PropertyIds::Node);

        Helpers::fixOverlap(rootContainer, &um);

        addAndMakeVisible(rootComponent = new ContainerComponent(rootContainer, &um));

		rootSizeListener.setCallback(rootContainer,
			{ UIPropertyIds::width, UIPropertyIds::height },
			Helpers::UIMode,
			VT_BIND_PROPERTY_LISTENER(onContainerResize));

        auto b = Helpers::getBounds(rootContainer);
        setSize(b.getWidth(), b.getHeight());
    }

    void resized() override
    {
        
    }

    void onContainerResize(const Identifier& id, const var& newValue)
    {
        auto b = Helpers::getBounds(rootComponent->getValueTree());
        setSize(b.getWidth(), b.getHeight());
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xFF222222));
        GlobalHiseLookAndFeel::draw1PixelGrid(g, this, getLocalBounds());
    }

    ValueTree data;

    UndoManager um;
    ScopedPointer<NodeComponent> rootComponent;
    valuetree::PropertyListener rootSizeListener;
};

}

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::Component,
                       public PathFactory
{
    static constexpr int MenuHeight = 24;

public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    // Your private member variables go here...

    Path createPath(const String& url) const override;
    
    hise::ZoomableViewport viewport;

    hise::HiseShapeButton openButton;
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
