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
                
                type = ContainerType::Split;


                for(int i = 0; i < numNodes; i++)
                {
                    MultiChannelConnection mc;

                    for(int c = 0; c < numChannels; c++)
                        mc.push_back({c, c, i == 1});

                    connections.push_back(mc);
                }
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
        
        static void drawCable(Graphics& g, Point<float> p1, Point<float> p2, bool cubic)
        {
            Path p;
            
            p.startNewSubPath(p1);
            
            if(!cubic)
            {
                createCurve(p, p1, p2, {0.0f, 0.0f }, true);
//                p.lineTo(p2);
                
                Path dashed;
                float d[2] = { 4.0f, 4.0f };
                
                PathStrokeType(0.5f).createDashedStroke(dashed, p, d, 2);
                std::swap(p, dashed);
            }
            else
            {
                createCurve(p, p1, p2, {0.0f, 0.0f }, true);
            }
 
            g.strokePath(p, PathStrokeType(1.0f));
        }

        static void createCurve(Path& path, Point<float> start, Point<float> end, Point<float> velocity, bool useCubic)
        {
               float dx = end.x - start.x;
               float absDx = std::abs(dx);

               // Minimum outward bend when wiring backward
               float minOffset = 80.0f;

               // Use half-distance going forward, otherwise give it some breathing room
               float controlOffset = dx > 10 ? absDx * 0.5f : minOffset;

               // Control points (bend horizontally)
               juce::Point<float> cp1 (start.x + controlOffset, start.y);
               juce::Point<float> cp2 (end.x - controlOffset, end.y);

               path.cubicTo (cp1, cp2, end);
        }
        
#if 0
        static void createCurve(Path& cable, Point<float> sourcePoint, Point<float> targetPoint, Point<float> velocity, bool useCubic)
        {
            auto controlPointOffset = (targetPoint.getX() - sourcePoint.getX()) * 0.5f; // 50% of the horizontal distance
                
            juce::Point<float> cp1 (sourcePoint.getX() + controlPointOffset, sourcePoint.getY());
            juce::Point<float> cp2 (targetPoint.getX() - controlPointOffset, targetPoint.getY());
            

            cable.cubicTo (cp1, cp2, targetPoint);
            
            return;
            
            auto mid = Rectangle<float>(sourcePoint, targetPoint).toFloat().getCentre().toFloat();
            
            auto c1 = mid.withY(sourcePoint.getY());
            auto c2 = mid.withY(targetPoint.getY());
            
            if(useCubic)
            {
                auto s2 = sourcePoint.translated(20.0f, 0.0f);
                auto t2 = targetPoint.translated(-20.0f, 0.0f);
                
                cable.cubicTo(s2, c1, mid);
                cable.cubicTo(c2, t2, targetPoint);
                
            }
            else
            {
                auto c1 = targetPoint.withY(sourcePoint.getY());

                cable.lineTo(c1);
                cable.lineTo(targetPoint);
            }
        }

#endif
        
        void draw(Graphics& g, const Pins& pinPositions)
        {

            
            if(pinPositions.childPositions.empty())
                return;
            
            const auto delta = (float)(SignalHeight) / (float)numChannels;
            auto xOffset = 10.0f;

            for(int c = 0; c < numChannels; c++)
            {
                Rectangle<float> s(pinPositions.containerStart, pinPositions.containerStart);
                Rectangle<float> e(pinPositions.containerEnd, pinPositions.containerEnd);
                g.fillEllipse(s.withSizeKeepingCentre(5.0f, 5.0f).translated(xOffset, c * delta + delta * 0.5f));
                g.fillEllipse(e.withSizeKeepingCentre(5.0f, 5.0f).translated(-xOffset, c * delta + delta * 0.5f));
            }
            
            if(type == ContainerType::Serial)
            {
                for(int i = 0; i < pinPositions.childPositions.size(); i++)
                {
                    float offset = delta * 0.5f;

                    for(int c = 0; c < numChannels; c++)
                    {
						auto p1 = i == 0 ? pinPositions.containerStart.translated(xOffset, 0.0f) : 
                                           pinPositions.childPositions[i - 1].second.translated(0.0f, 0.0f);

						auto p2 = pinPositions.childPositions[i].first;
						drawCable(g, p1.translated(0, offset), p2.translated(0.0f, offset), false);
                        offset += delta;
                    }
                }

				float offset = delta * 0.5f;

				for (int c = 0; c < numChannels; c++)
				{
					auto p1 = pinPositions.childPositions.back().second;
					auto p2 = pinPositions.containerEnd;
					drawCable(g, p1.translated(0.0f, offset), p2.translated(-xOffset, offset), false);
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
                        auto p2 = pinPositions.childPositions[i].first.translated(0, offset);
                        auto p3 = pinPositions.childPositions[i].second.translated(0, offset);
                        auto p4 = pinPositions.containerEnd.translated(-xOffset, offset);
                        drawCable(g, p1, p2, false);
                        drawCable(g, p3, p4, false);
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
    static constexpr int ParameterHeight = 32;
    static constexpr int ParameterWidth = 128;

    static bool isProcessNode(const ValueTree& v)
    {
        jassert(v.getType() == PropertyIds::Node);
        auto path = v[PropertyIds::FactoryPath].toString();
            
        auto factory = path.upToFirstOccurrenceOf(".", false, false);
        auto nid = path.fromFirstOccurrenceOf(".", false, false);
        
        if(factory == "control")
            return false;
        
        if(nid.contains("_cable"))
            return false;
        
        return true;
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

        return false;
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

        auto minY = 10 + HeaderHeight;

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
					cb = cb.withY(fullBounds.getBottom() + 10);
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
            jmax(currentWidth, fullBounds.getRight() + NodeMargin, bounds.getWidth()),
            jmax(currentHeight, fullBounds.getBottom() + NodeMargin, bounds.getHeight()),
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

    static void fixOverlap(ValueTree v, UndoManager* um)
    {
        fixOverlapRecursive(v, um);
    }

    static Colour getNodeColour(const ValueTree& v)
    {
        jassert(v.getType() == PropertyIds::Node);

        
        auto t = v[PropertyIds::FactoryPath].toString();
        
        if(t == "container.midichain")
            return Colour(0xFFC65638);
        if(t == "container.modchain")
            return Colour(0xffbe952c);
        
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

        //text << " (" << v[PropertyIds::FactoryPath].toString() << ")";

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
            w = 200;
        if(h == 0)
            h = 100;

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
        {
            addAndMakeVisible(powerButton);
        };

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
            b.removeFromLeft(getHeight());
			g.drawText(Helpers::getHeaderTitle(parent.getValueTree()), b, Justification::left);
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

    struct ParameterComponent: public Component
    {
        ParameterComponent(const ValueTree& v, UndoManager* um_):
          data(v),
          um(um_)
        {
            addAndMakeVisible(slider);
            slider.setLookAndFeel(&laf);
            slider.setRange(0.0, 1.0, 0.0);
            slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, false, 0, 0);

            Colour tr = Colours::transparentBlack;
            slider.setColour(HiseColourScheme::ColourIds::ComponentFillTopColourId, tr);
            slider.setColour(HiseColourScheme::ColourIds::ComponentFillBottomColourId, tr);
            slider.setColour(HiseColourScheme::ColourIds::ComponentOutlineColourId, tr);
        };
        
        void paint(Graphics& g) override
        {
            auto name = data[PropertyIds::ID].toString();
            g.setFont(GLOBAL_BOLD_FONT());
            g.setColour(Colours::white);
            auto tb = getLocalBounds().toFloat();
            tb.removeFromLeft(Helpers::ParameterHeight + 5.0f);
            g.drawText(name, tb, Justification::left);
        }
        
        
        hise::GlobalHiseLookAndFeel laf;
        std::vector<ParameterComponent*> targets;

        bool rebuildTargets(NodeComponent* parent)
        {
            targets.clear();
            
            Component::callRecursive<ParameterComponent>(parent, [&](ParameterComponent* c)
            {
                auto p1 = c->data[PropertyIds::ID].toString();
                auto n1 = c->data.getParent().getParent()[PropertyIds::ID].toString();
                
                for(auto d: data.getChildWithName(PropertyIds::Connections))
                {
                    auto p2 = d[PropertyIds::ParameterId].toString();
                    auto n2 = d[PropertyIds::NodeId].toString();
                    
                    auto pMatch = p1 == p2;
                    auto nMatch = n1 == n2;
                    
                    if(pMatch && nMatch)
                    {
                        targets.push_back(c);
                        break;
                    }
                }

                return false;
            });
            
            return !targets.empty();
        }
        
        void resized() override
        {
            slider.setBounds(getLocalBounds().removeFromLeft(Helpers::ParameterHeight));
        }
        
        ValueTree data;
        UndoManager* um;
        juce::Slider slider;
    };
    
    
    struct ModOutputComponent: public Component
    {
        ModOutputComponent(const ValueTree& v, UndoManager* um_):
          data(v),
          um(um_)
        {
        }
        
        void paint(Graphics& g) override
        {
            g.setColour(Colours::white);
            g.setFont(GLOBAL_BOLD_FONT());
            
            auto b = getLocalBounds().toFloat();
            b.removeFromRight(20);
            
            if(data.getType() == PropertyIds::ModulationTargets)
            {
                g.drawText("Output", b, Justification::right);
            }
            else
            {
                auto idx = data.getParent().indexOf(data);
                g.drawText("Output "+ String(idx+1), b, Justification::right);
            }
        }
        
        std::vector<ParameterComponent*> targets;

        bool rebuildTargets(NodeComponent* parent)
        {
            targets.clear();
            
            Component::callRecursive<ParameterComponent>(parent, [&](ParameterComponent* c)
            {
                auto p1 = c->data[PropertyIds::ID].toString();
                auto n1 = c->data.getParent().getParent()[PropertyIds::ID].toString();
                
                auto conTree = data;
                if(conTree.getType() == PropertyIds::SwitchTarget)
                    conTree = data.getChildWithName(PropertyIds::Connections);
                
                for(auto d: conTree)
                {
                    auto p2 = d[PropertyIds::ParameterId].toString();
                    auto n2 = d[PropertyIds::NodeId].toString();
                    
                    auto pMatch = p1 == p2;
                    auto nMatch = n1 == n2;
                    
                    if(pMatch && nMatch)
                    {
                        targets.push_back(c);
                        break;
                    }
                }

                return false;
            });
            
            return !targets.empty();
        }
        
        ValueTree data;
        UndoManager* um;
            
    };
    
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
        
        for(auto p: data.getChildWithName(PropertyIds::Parameters))
        {
            addAndMakeVisible(parameters.add(new ParameterComponent(p, um)));
        }
          
        auto modOutput = data.getChildWithName(PropertyIds::ModulationTargets);
          
        if(modOutput.isValid())
        {
            addAndMakeVisible(modOutputs.add(new ModOutputComponent(modOutput, um)));
        }
        
        for(auto st: data.getChildWithName(PropertyIds::SwitchTargets))
            addAndMakeVisible(modOutputs.add(new ModOutputComponent(st, um)));
        
        resized();
    };

    struct CableComponent: public Component
    {
        CableComponent(NodeComponent& parent, Point<float> start, Point<float> end)
        {
            setInterceptsMouseClicks(false, false);
            Rectangle<float> a(start, end);
            setBounds(a.toNearestInt().expanded(22));
            setRepaintsOnMouseActivity(true);
        }
        
        bool hitTest(int x, int y) override
        {
            Point<float> sp((float)x, (float)y);
            Point<float> tp;
            p.getNearestPoint(sp, tp);
            
            return sp.getDistanceFrom(tp) < 20.0f;
        }
        
        void setCablePoints(NodeComponent& parent, Point<float> start, Point<float> end)
        {
            s = getLocalPoint(&parent, start);
            e = getLocalPoint(&parent, end);
            
            p.startNewSubPath(s);
                        
            Helpers::CableSetup::createCurve(p, s, e.translated(-3.0f, 0.0f), {0.0f, 0.0f }, true);
        }
        
        void mouseMove(const MouseEvent& e) override
        {
            over = contains(e.getPosition());
            repaint();
        }
        
        void mouseExit(const MouseEvent& e) override
        {
            over = false;
            repaint();
        }
        
        bool over = false;
        
        Point<float> s, e;
        Path p;
        
        void paint(Graphics& g) override
        {
            g.setColour(colour);
            g.strokePath(p, PathStrokeType(over ? 3.0f : 1.0f));
            Path arrow;
            g.fillEllipse(Rectangle<float>(s, s).withSizeKeepingCentre(6.0f, 6.0f));
            arrow.startNewSubPath(e);
            arrow.lineTo(e.translated(-7.0f, 5.0f));
            arrow.lineTo(e.translated(-7.0f, -5.0f));
            arrow.closeSubPath();
            g.fillPath(arrow);
        }
        
        Colour colour;
    };
    
    OwnedArray<CableComponent> cables;
    
    void createParameterCables()
    {
        for(auto src: parameters)
        {
            for(auto dst: src->targets)
            {
                auto start = getLocalArea(src, src->getLocalBounds()).toFloat();
                Point<float> s( start.getRight(), start.getCentreY());
                auto end = getLocalArea(dst, dst->getLocalBounds()).toFloat();
                Point<float> e( end.getX(), end.getCentreY());

                auto colour = Helpers::getNodeColour(dst->data.getParent().getParent());
                
                if(colour.isTransparent())
                    colour = Colours::grey;
                
                auto nc = new CableComponent(*this, s, e);
                
                cables.add(nc);
                addAndMakeVisible(nc);
                nc->setCablePoints(*this, s, e);
                nc->colour = colour;
            }
        }
    }
    
    void createModCables(NodeComponent* parent)
    {
        for(auto src: modOutputs)
        {
            for(auto dst: src->targets)
            {
                auto start = parent->getLocalArea(src, src->getLocalBounds()).toFloat();
                Point<float> s( start.getRight(), start.getCentreY());
                auto end = parent->getLocalArea(dst, dst->getLocalBounds()).toFloat();
                Point<float> e( end.getX(), end.getCentreY());

                auto colour = Helpers::getNodeColour(dst->data.getParent().getParent());
                
                if(colour.isTransparent())
                    colour = Colours::grey;

                
                auto nc = new CableComponent(*parent, s, e);
                parent->cables.add(nc);
                nc->colour = colour;
                parent->addAndMakeVisible(nc);
                nc->setCablePoints(*parent, s, e);
            }
        }
    }
    
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
        
        auto pb = b.removeFromLeft(Helpers::ParameterWidth);
        
        auto mb = b.removeFromRight(Helpers::ParameterWidth);
        
        for(int i = parameters.size()-1; i >= 0; i--)
        {
            auto p = parameters[i];
            p->setBounds(pb.removeFromBottom(Helpers::ParameterHeight));
            pb.removeFromBottom(5);
        }
        
        for(auto m: modOutputs)
        {
            m->setBounds(mb.removeFromTop(24));
        }
    }

    ValueTree& getValueTree() { return data; }

    SignalComponent* getSignalComponent() const { return signalComponent.get(); }

    bool rebuildModTargets(NodeComponent* n)
    {
        auto ok = false;
        
        for(auto m: modOutputs)
            ok |= m->rebuildTargets(n);
        
        return ok;
    }
    
    void setFixSize(Rectangle<int> extraBounds)
    {
        int x = 0;
        
        if(!parameters.isEmpty())
            x += Helpers::ParameterWidth;
        
        x += extraBounds.getWidth();
        
        if(!modOutputs.isEmpty())
            x += Helpers::ParameterWidth;
        
        int y = Helpers::HeaderHeight;
        
        if(signalComponent != nullptr)
            y += Helpers::SignalHeight;
                
        auto bodyHeight = jmax(extraBounds.getHeight(), parameters.size() * (Helpers::ParameterHeight+5), modOutputs.size() * 24);

        Rectangle<int> nb(getPosition(), getPosition().translated(x, y + bodyHeight));
        
        Helpers::updateBounds(getValueTree(), nb, um);
        
        setSize(x, y + bodyHeight);
    }
    
protected:

    OwnedArray<ParameterComponent> parameters;
    OwnedArray<ModOutputComponent> modOutputs;
    
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
        setFixSize({});
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
        setFixSize({});
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

        setInterceptsMouseClicks(false, true);
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
        auto nodeColour = Helpers::getNodeColour(data);

		g.setColour(nodeColour);
        g.drawRect(getLocalBounds(), 1);

        g.fillAll(Colour(0x77222222));
        
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
        
        g.setColour(Helpers::getNodeColour(data));
        cs.draw(g, pins);

    }
    
    Image cableImage;

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

    void rebuildCables()
    {
        auto ok = false;
        
        for(auto cn: childNodes)
            ok |= cn->rebuildModTargets(this);
        
        for(auto p: parameters)
            ok |= p->rebuildTargets(this);
        
        cables.clear();
        
        if(ok)
        {
            for(auto cn: childNodes)
                cn->createModCables(this);
            
            createParameterCables();

            for(auto c: cables)
                c->toBack();
        }
    }
    
	void resized() override
	{
        NodeComponent::resized();

        
        
        rebuildCables();
        
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

        Helpers::fixOverlap(rootContainer, &um);
        
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
