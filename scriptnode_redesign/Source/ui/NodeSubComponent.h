/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#pragma once


namespace scriptnode {
using namespace hise;
using namespace juce;


struct RangePresets
{
	RangePresets();

	static File getRangePresetFile();

	void createDefaultRange(const String& id, InvertableParameterRange d, double midPoint = -10000000.0);

	int getPresetIndex(const InvertableParameterRange& r)
	{
		for (int i = 0; i < presets.size(); i++)
		{
			if (presets[i].nr == r)
				return i;
		}

		return -1;
	}

	~RangePresets();

	struct Preset
	{
		void restoreFromValueTree(const ValueTree& v);

		ValueTree exportAsValueTree() const;

		InvertableParameterRange nr;
		String id;
		int index;
	};

	File fileToLoad;
	Array<Preset> presets;
};

struct CablePinBase : public Component,
					  public PathFactory
{
	using WeakPtr = WeakReference<CablePinBase>;
	using Connections = std::vector<WeakPtr>;

	virtual ~CablePinBase() = default;

    virtual Helpers::ConnectionType getConnectionType() const = 0;

    virtual bool matchesConnectionType(Helpers::ConnectionType ct) const = 0;

    virtual bool canBeTarget() const = 0;

	CablePinBase(const ValueTree& v, UndoManager* um_) :
		data(v),
		um(um_),
		targetIcon(createPath("target"))
	{};

	Path createPath(const String& url) const override
	{
		Path p;
		LOAD_EPATH_IF_URL("target", ColumnIcons::targetIcon);
		return p;
	}

	bool isDraggable() const { return draggingEnabled; }

	virtual String getSourceDescription() const = 0;

    virtual String getTargetParameterId() const = 0;
    virtual ValueTree getConnectionTree() const = 0;

    ValueTree getNodeTree() const 
    {
        if(data.getType() == PropertyIds::Node)
            return data;

        return Helpers::findParentNode(data);
    }

    bool isFoldedAway() const
    {
        return Helpers::isFoldedRecursive(getNodeTree().getParent().getParent());
    }
    
    /** Override this method and check if the value tree for the given connection
        matches this component as source. */
    virtual bool matchConnection(const ValueTree& possibleConnection) const
    {
		auto p1 = getTargetParameterId();
		auto n1 = getNodeTree()[PropertyIds::ID].toString();

		auto p2 = possibleConnection[PropertyIds::ParameterId].toString();
		auto n2 = possibleConnection[PropertyIds::NodeId].toString();

		auto pMatch = p1 == p2;
		auto nMatch = n1 == n2;

        return pMatch && nMatch;
    }

	virtual Result getTargetErrorMessage(const ValueTree& requestedSource) const
	{
		if(!canBeTarget())
			return Result::fail("Not a connection target");

		if(data[PropertyIds::Automated])
			return Result::fail("Already connected");

		return Result::ok();
	}

	Connections rebuildConnections(Component* parent);

	void mouseDown(const MouseEvent& e) override;

	void mouseDrag(const MouseEvent& e) override;

	void mouseUp(const MouseEvent& e) override;

	void setEnableDragging(bool shouldBeDragable);

	void addConnection(const WeakPtr dst);

	void setBlinkAlpha(float newAlpha)
	{
		blinkAlpha = jlimit(0.0f, 1.0f, newAlpha);
		repaint();

	}

	void drawBlinkState(Graphics& g) 
	{
		if(blinkAlpha > 0.0f)
		{
			g.setColour(Colours::white.withAlpha(jlimit(0.0f, 1.0f, blinkAlpha)));
			g.drawRoundedRectangle(getLocalBounds().toFloat(), 3.0f, 2.0f);

			g.setColour(Colours::white.withAlpha(jlimit(0.0f, 1.0f, blinkAlpha) * 0.2f));
			g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.0f);
		}
	}

	ValueTree data;
	UndoManager* um;

protected:

	float blinkAlpha = 0.0f;
	Path targetIcon;
	bool draggingEnabled = false;

	JUCE_DECLARE_WEAK_REFERENCEABLE(CablePinBase);
};

struct ParameterComponent : public CablePinBase,
							public Slider::Listener,
							public PooledUIUpdater::SimpleTimer
{
	struct Laf: public GlobalHiseLookAndFeel
	{
		void drawButtonBackground(Graphics& g, Button& button, const Colour& , bool isMouseOverButton, bool isButtonDown) override
		{
			float alpha = 0.05f;

			if(isMouseOverButton)
				alpha += 0.05f;

			if(isButtonDown)
				alpha += 0.1f;

			g.setColour(Colours::white.withAlpha(alpha));
			g.fillRoundedRectangle(button.getLocalBounds().toFloat().reduced(2, 1), 3.0f);
		}

		void drawRotarySlider(Graphics &g, int x, int y, int width, int height, float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, Slider &s)
		{
			auto lod = LODManager::getLOD(s);

			if(lod <= 1)
			{
				GlobalHiseLookAndFeel::drawRotarySlider(g, x, y, width, height, sliderPosProportional, rotaryStartAngle, rotaryEndAngle, s);
			}
			else
			{


				g.setColour(Colour(0xFF111118));

				Path track, value;
				track.startNewSubPath({0.0f, 0.0f});
				track.startNewSubPath({1.0f, 1.0f});
				value.startNewSubPath({ 0.0f, 0.0f });
				value.startNewSubPath({ 1.0f, 1.0f });

				track.addArc(0.0f, 0.0f, 1.0f, 1.0f, rotaryStartAngle, rotaryEndAngle, true);
				value.addArc(0.0f, 0.0f, 1.0f, 1.0f, rotaryStartAngle, rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle), true);

				track.scaleToFit(x+5, y+5, width-10, height-10, true);
				value.scaleToFit(x+5, y+5, width-10, height-10, true);

				g.strokePath(track, PathStrokeType(5.0f));

				auto down = s.isMouseButtonDown();

				auto rColour = (down ? 0xFF9099AA : 0xFF808899);
				auto c = Colour(rColour);
				auto ringColour = c.withAlpha(0.5f + sliderPosProportional * 0.5f);
				
				g.setColour(ringColour);
				g.strokePath(value, PathStrokeType(3.0f));


				//g.fillEllipse(s.getLocalBounds().reduced(4.0f).toFloat());
			}
		}

		void drawButtonText(Graphics &g, TextButton &button, bool over, bool down) override
		{
			float alpha = 0.4f;

			if (over)
				alpha += 0.2f;
			if (down)
				alpha += 0.1f;

			g.setColour(Colours::white.withAlpha(alpha));

			if(button.isToggleable())
			{
				auto b = button.getLocalBounds().reduced(5.0f).toFloat();

				auto c = b.removeFromRight(b.getHeight());

				if(button.getToggleState())
					g.fillEllipse(c);
				else
					g.drawEllipse(c, 1.0f);
			}
			else
			{
				Path p;

				p.addTriangle({ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.5f, 1.0f });

				auto b = button.getLocalBounds().reduced(5.0f).toFloat();

				PathFactory::scalePath(p, b.removeFromRight(13.0f));

				
				g.fillPath(p);
			}
			
		}
	} laf;

	struct RangeComponent;

	ParameterComponent(PooledUIUpdater* updater, ValueTree v, UndoManager* um_) :
		CablePinBase(v, um_),
		SimpleTimer(updater, false),
		dropDown("")
	{
		jassert(v.getType() == PropertyIds::Parameter);
		addAndMakeVisible(slider);
		slider.setLookAndFeel(&laf);

		auto nr = scriptnode::RangeHelpers::getDoubleRange(v);

		slider.setNormalisableRange(nr.rng);
		slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
		slider.setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, false, 0, 0);

		Colour tr = Colours::transparentBlack;
		slider.setColour(HiseColourScheme::ColourIds::ComponentFillTopColourId, tr);
		slider.setColour(HiseColourScheme::ColourIds::ComponentFillBottomColourId, tr);
		slider.setColour(HiseColourScheme::ColourIds::ComponentOutlineColourId, tr);
		slider.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
		slider.setScrollWheelEnabled(false);

		

		slider.addListener(this);

		rangeUpdater.setCallback(data, RangeHelpers::getRangeIds(), Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onRange));
		

		automationUpdater.setCallback(data, { PropertyIds::Automated }, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onAutomated));
		onAutomated({}, data[PropertyIds::Automated]);

		auto vtc = ValueToTextConverter::fromString(v[PropertyIds::TextToValueConverter].toString());

		if(vtc.active)
		{
			slider.textFromValueFunction = vtc;
			slider.valueFromTextFunction = vtc;
		}

		addChildComponent(dropDown);

		if(!vtc.itemList.isEmpty())
		{
			dropDown.setLookAndFeel(&laf);
			dropDown.setVisible(true);

			if(vtc.itemList.size() == 2)
			{
				auto on = (int)data[PropertyIds::Value];
				dropDown.setButtonText(vtc.itemList[on]);
				dropDown.setClickingTogglesState(true);
				dropDown.setToggleState(on, dontSendNotification);
				
				dropDown.onClick = [this, vtc]()
				{
					auto v = 1 - (int)dropDown.getToggleState();
					data.setProperty(PropertyIds::Value, 1 - v, um);
					dropDown.setButtonText(vtc.itemList[v]);
				};
			}
			else
			{
				dropDown.onClick = [this, vtc]()
				{
					PopupMenu m;
					m.setLookAndFeel(&laf);

					auto value = (int)this->data[PropertyIds::Value];

					bool automated = false;

					if(this->data[PropertyIds::Automated])
					{
						automated = true;
						value = (int)this->lastValue;
					}
						

					for (int i = 0; i < vtc.itemList.size(); i++)
					{
						auto s = vtc.itemList[i];
						s << " (" << String(i) << ')';

						m.addItem(1 + i, s, !automated, i == value);
					}

					auto r = m.showAt(&dropDown);

					if (r != 0)
					{
						data.setProperty(PropertyIds::Value, r - 1, um);
						repaint();
					}
				};
			}

			
			slider.setVisible(false);
		}

		setSize(Helpers::ParameterWidth, Helpers::ParameterHeight);

		auto parentNode = valuetree::Helpers::findParentWithType(v, PropertyIds::Node);

		if(Helpers::isContainerNode(parentNode) && !Helpers::isFoldedOrLockedContainer(parentNode))
		{
			NodeDatabase db;
			
			auto hasFixed = db.getProperties(parentNode[PropertyIds::FactoryPath])->hasProperty(PropertyIds::HasFixedParameters);

			setEnableDragging(!hasFixed);

			if (!v.hasProperty(PropertyIds::NodeColour))
			{
				auto c = ParameterHelpers::getParameterColour(v);
				v.setProperty(PropertyIds::NodeColour, c.getARGB(), um);
			}
		}
	};

	void sliderValueChanged(Slider* s) override
	{
		repaint();
	}

	double lastValue = 0;

	void timerCallback() override
	{
		if(auto pc = findParentComponentOfClass<scriptnode::NodeComponentParameterSource>())
		{
			auto idx = data.getParent().indexOf(data);
			auto nv = pc->getParameterValue(idx);

			if(lastValue != nv)
			{
				lastValue = nv;
				slider.setValue(lastValue, dontSendNotification);

				if(dropDown.isToggleable())
					dropDown.setToggleState(lastValue > 0.5, dontSendNotification);

				repaint();
			}
		}
	}

	void onAutomated(const Identifier& id, const var& newValue)
	{
		if((bool)newValue)
		{
			start();
			slider.setEnabled(false);
			slider.getValueObject().referTo({});
		}
		else
		{
			stop();
			slider.setEnabled(true);
			slider.getValueObject().referTo(data.getPropertyAsValue(PropertyIds::Value, um, false));
		}
	}

	bool isOutsideParameter() const;

	void paint(Graphics& g) override
	{
		LODManager::LODGraphics lg(g, *this);

		if (draggingEnabled)
		{
			g.setColour(ParameterHelpers::getParameterColour(data));

			lg.fillRoundedRectangle(getLocalBounds().toFloat().reduced(3.0f, 0.0f).removeFromRight(4.0f), 2.0f);

			g.setColour(Colours::white.withAlpha(0.5f));
			auto pb = getLocalBounds().removeFromRight(getHeight()).toFloat().reduced(10.0f);
			scalePath(targetIcon, pb);
			lg.fillPath(targetIcon);
		}

		drawBlinkState(g);

		auto name = data[PropertyIds::ID].toString();
		
		
		auto tb = getLocalBounds().toFloat();

		tb.removeFromLeft(Helpers::ParameterMargin);

		if(slider.isVisible())
			tb.removeFromLeft(Helpers::ParameterHeight);
		
		if(isOutsideParameter())
		{
			auto sourceNode = valuetree::Helpers::findParentWithType(data, PropertyIds::Node);

			g.setFont(GLOBAL_FONT());
			auto rootId = Helpers::getHeaderTitle(sourceNode);
			auto c = Helpers::getNodeColour(sourceNode);
			
			auto nb = tb.removeFromTop(tb.getHeight() * 0.5).reduced(1.0f);

			nb.removeFromRight(getHeight());

			auto tb = nb;

			g.setColour(c.withAlpha(0.2f));
			g.fillRect(nb);
			g.setColour(c);
			g.fillRect(nb.removeFromLeft(2));
			

			g.setColour(Colours::white.withAlpha(0.5f));

			lg.drawText(rootId, nb.reduced(3.0f, 0.0f), Justification::left);
		}
		else
		{
			g.setFont(GLOBAL_FONT());
			g.setColour(Colours::white.withAlpha(0.6f));

			lg.drawText(slider.getTextFromValue(slider.getValue()), tb.reduced(0.0f, 2.0f), Justification::bottomLeft);
		}
		
		g.setFont(GLOBAL_BOLD_FONT());
		g.setColour(Colours::white.withAlpha(0.8f));
		
		lg.drawText(name, tb.reduced(0.0f, 2.0f), Justification::topLeft);
	}

	Helpers::ConnectionType getConnectionType() const override { return Helpers::ConnectionType::Parameter; };

	bool matchesConnectionType(Helpers::ConnectionType ct) const override
	{
		// matches all types except for routable signals (bypass, modulation, parameter)
		return ct != Helpers::ConnectionType::RoutableSignal;
	};

	bool canBeTarget() const override { return true; }

	void mouseDown(const MouseEvent& e) override;

	String getTargetParameterId() const override { return data[PropertyIds::ID].toString(); }

	Result getTargetErrorMessage(const ValueTree& requestedSource) const override
	{
		auto sourceParent = Helpers::findParentNode(requestedSource);

		if(!data.isAChildOf(sourceParent) && requestedSource.getType() == PropertyIds::Parameter)
			return Result::fail("Can't connect container parameter to outside node");

		return CablePinBase::getTargetErrorMessage(requestedSource);
	}

	String getSourceDescription() const override { return Helpers::getHeaderTitle(getNodeTree()) + "." + getTargetParameterId(); }

	ValueTree getConnectionTree() const override
	{
		return data.getChildWithName(PropertyIds::Connections);
	}

	void resized() override
	{
		auto b = getLocalBounds();

		if(slider.isVisible())
			slider.setBounds(b.removeFromLeft(Helpers::ParameterHeight));
		else
		{
			b.removeFromRight(Helpers::ParameterMargin);
			dropDown.setBounds(b.removeFromBottom(b.getHeight() / 2));
		}
	}

	void onRange(const Identifier&, const var&)
	{
		auto nr = RangeHelpers::getDoubleRange(data);
		slider.setNormalisableRange(nr.rng);
	}

	juce::Slider slider;

	valuetree::PropertyListener rangeUpdater;
	valuetree::PropertyListener automationUpdater;

	TextButton dropDown;
};

struct LockedTarget: public CablePinBase
{
	LockedTarget(const ValueTree& v, UndoManager* um):
	  CablePinBase(v, um)
	{
		jassert(v.getType() == PropertyIds::Parameter);
		setSize(Helpers::ParameterWidth, Helpers::ModulationOutputHeight);
	}

	Helpers::ConnectionType getConnectionType() const override { return Helpers::ConnectionType::Parameter; };

	bool matchesConnectionType(Helpers::ConnectionType ct) const override
	{
		return ct != Helpers::ConnectionType::RoutableSignal;
	}

	String getSourceDescription() const override { return Helpers::getHeaderTitle(getNodeTree()) + "." + getTargetParameterId(); }

	bool canBeTarget() const override { return true; }

	String getTargetParameterId() const override { return data[PropertyIds::ID].toString(); }

	void paint(Graphics& g) override
	{
		drawBlinkState(g);

		g.setColour(Colours::white.withAlpha(0.5f));
		g.setFont(GLOBAL_FONT());
		
		LODManager::LODGraphics lg(g, *this);
		lg.drawText(getSourceDescription(), getLocalBounds().toFloat().reduced(5.0f, 0.0f), Justification::left);
	}

	ValueTree getConnectionTree() const override
	{
		jassertfalse;
		return {};
	}
};

struct ModulationBridge: public CablePinBase
{
	ModulationBridge(const ValueTree& v, UndoManager* um):
	  CablePinBase(v, um)
	{
		jassert(v.getType() == PropertyIds::ModulationTargets || v.getType() == PropertyIds::SwitchTarget);

		setEnableDragging(true);
		setRepaintsOnMouseActivity(true);

		setSize(Helpers::ParameterWidth, Helpers::ParameterHeight);
	}

	Helpers::ConnectionType getConnectionType() const override { return Helpers::ConnectionType::Modulation; };

	bool matchesConnectionType(Helpers::ConnectionType ct) const override
	{
		// can only connect to parameters
		return ct == Helpers::ConnectionType::Parameter;
	};

	String getSourceDescription() const override 
	{ 
		String label;
		label << Helpers::getHeaderTitle(getNodeTree());
		label << ".Output";

		if (data.getType() == PropertyIds::SwitchTarget)
			label << " " + String(data.getParent().indexOf(data) + 1);

		return label;
	}

	void paint(Graphics& g) override
	{
		g.setColour(Colours::white.withAlpha(isMouseOver() ? 0.8f : 0.5f));
		g.setFont(GLOBAL_BOLD_FONT());

		drawBlinkState(g);

		auto sourceNode = valuetree::Helpers::findParentWithType(data, PropertyIds::Node);

		auto c = Helpers::getNodeColour(sourceNode);

		LODManager::LODGraphics lg(g, *this);

		g.setColour(c);

		lg.fillRoundedRectangle(getLocalBounds().toFloat().reduced(3.0f, 0.0f).removeFromRight(4.0f), 2.0f);

		auto b = getLocalBounds().toFloat();
		auto tb = b.removeFromRight(getHeight()).toFloat();

		scalePath(targetIcon, tb.reduced(10.0f));

		lg.fillPath(targetIcon);

		auto rb = b.reduced(0, 5);

		g.setColour(c.withAlpha(0.05f));
		g.fillRect(rb);
		g.setColour(c);
		g.fillRect(rb.removeFromLeft(2));

		g.setColour(Colours::white.withAlpha(0.5f));

		String label = "from " + getSourceDescription();
		b.removeFromLeft(5);

		lg.drawText(label, b, Justification::left);
	}

	bool canBeTarget() const override { return true; }
	String getTargetParameterId() const override { return {}; }

	ValueTree getConnectionTree() const override
	{
		auto conTree = data;
		if (conTree.getType() == PropertyIds::SwitchTarget)
			conTree = data.getChildWithName(PropertyIds::Connections);

		return conTree;
	}
};

struct ModOutputComponent : public CablePinBase
{
	ModOutputComponent(const ValueTree& v, UndoManager* um_) :
		CablePinBase(v, um_)
	{
		setEnableDragging(true);
		setRepaintsOnMouseActivity(true);
	}

	Helpers::ConnectionType getConnectionType() const override { return Helpers::ConnectionType::Modulation; };

	String getSourceDescription() const override
	{
		String label;
		label << Helpers::getHeaderTitle(getNodeTree());

		if (data.hasProperty(PropertyIds::ID))
		{
			label << "." << data[PropertyIds::ID].toString();
		}
		else
		{
			label << ".Output";

			if (data.getType() == PropertyIds::SwitchTarget)
				label << " " + String(data.getParent().indexOf(data) + 1);
		}

		return label;
	}

	bool matchesConnectionType(Helpers::ConnectionType ct) const override
	{
		// can only connect to parameters
		return ct == Helpers::ConnectionType::Parameter;
	};

	bool canBeTarget() const override { return false; }
	String getTargetParameterId() const override { return {}; }

	void paint(Graphics& g) override
	{
		g.setColour(Colours::white.withAlpha(isMouseOver() ? 0.8f : 0.5f));
		g.setFont(GLOBAL_BOLD_FONT());

		LODManager::LODGraphics lg(g, *this);

		drawBlinkState(g);

		auto b = getLocalBounds().toFloat();
		auto tb = b.removeFromRight(getHeight()).toFloat();

		scalePath(targetIcon, tb.reduced(8.0f));

		lg.fillPath(targetIcon);
		lg.drawText(getSourceDescription().fromFirstOccurrenceOf(".", false, false), b, Justification::right);
	}

	ValueTree getConnectionTree() const override
	{
		auto conTree = data;
		if (conTree.getType() == PropertyIds::SwitchTarget)
			conTree = data.getChildWithName(PropertyIds::Connections);

		return conTree;
	}
};

struct FoldedInput: public ParameterComponent
{
	FoldedInput(PooledUIUpdater* updater, const ValueTree& v, UndoManager* um):
	  ParameterComponent(updater, v, um)
	{
		setSize(Helpers::ParameterWidth, Helpers::ParameterHeight);
		slider.setVisible(false);
	}

	void paint(Graphics& g) override
	{
		auto b = getLocalBounds().toFloat();

		drawBlinkState(g);

		g.setFont(GLOBAL_BOLD_FONT());
		g.setColour(Colours::white.withAlpha(0.2f));
		g.drawText(getTargetParameterId(), b, Justification::left);
	}
};

struct FoldedOutput : public ModOutputComponent
{
	FoldedOutput(const ValueTree& v, UndoManager* um):
	  ModOutputComponent(v, um)
	{
		setSize(Helpers::ParameterWidth, Helpers::ParameterHeight);
	}
};

struct BuildHelpers
{
	struct CreateData
	{
		ValueTree containerToInsert;
		WeakReference<CablePinBase> source;
		WeakReference<CablePinBase> target;
		Point<int> pointInContainer;
		int signalIndex = -1;
	};

	static void forEach(const ValueTree& root, const Identifier& typeId, const std::function<void(ValueTree&)>& f)
	{
		valuetree::Helpers::forEach(root, [&](ValueTree& v)
		{
			if(v.getType() == typeId)
				f(v);

			return false;
		});
	}

	static void cleanBeforeMove(const Array<ValueTree>& nodesToBeMoved, ValueTree& newParent, UndoManager* um)
	{
		StringArray nodeIds;

		auto newContainer = valuetree::Helpers::findParentWithType(newParent, PropertyIds::Node);

		for(auto vToBeMoved: nodesToBeMoved)
		{
			if (vToBeMoved.getType() == PropertyIds::Node)
			{
				nodeIds.add(vToBeMoved[PropertyIds::ID]);

				Array<ValueTree> connectionsToBeRemoved;

				// Remove parameter connections if the node is dragged outside the container with the parameter connection
				forEach(vToBeMoved, PropertyIds::Parameter, [&](ValueTree& p)
				{
					auto con = ParameterHelpers::getConnection(p);

					if (con.isValid())
					{
						auto isParameterConnection = con.getParent().getParent().getType() == PropertyIds::Parameter;

						if (isParameterConnection)
						{
							auto parameterContainer = valuetree::Helpers::findParentWithType(con, PropertyIds::Node);
							auto ok = newContainer == parameterContainer || newParent.isAChildOf(parameterContainer);

							if (!ok)
								connectionsToBeRemoved.add(con);
						}
					}
				});

				for (auto c : connectionsToBeRemoved)
				{
					cleanBeforeDelete(c, um);
					c.getParent().removeChild(c, um);
				}

				
			}
		}
		
		auto root = valuetree::Helpers::findParentWithType(newParent, PropertyIds::Network);
		
		forEach(root, PropertyIds::Connection, [&](ValueTree& con)
		{
			auto target = ParameterHelpers::getTarget(con);
			auto connectionParentNode = Helpers::findParentNode(con);

			for(auto& n: nodesToBeMoved)
			{
				if(target.isAChildOf(n))
				{
					auto hideCable = newParent != connectionParentNode;
					con.setProperty(UIPropertyIds::HideCable, hideCable, um);
					break;
				}
			}
		});

		auto removedGroups = removeFromGroups(root, nodeIds, um);

		if(!removedGroups.isEmpty())
		{
			auto groups = newContainer.getOrCreateChildWithName(UIPropertyIds::Groups, um);

			for(auto g: removedGroups)
				groups.addChild(g, -1, um);
		}
	}
	
	static Array<ValueTree> removeFromGroups(const ValueTree& root, const StringArray& nodeIds, UndoManager* um)
	{
		Array<ValueTree> groupsToDelete;

		forEach(root, UIPropertyIds::Group, [&](ValueTree& g)
			{
				auto idList = StringArray::fromTokens(g[PropertyIds::Value].toString(), ";", "");
				auto before = idList.size();

				for (auto& n : nodeIds)
					idList.removeString(n);

				if (idList.size() != before)
				{
					if (idList.isEmpty())
						groupsToDelete.add(g);
					else
						g.setProperty(PropertyIds::Value, idList.joinIntoString(";"), um);
				}
			});

		for (auto g : groupsToDelete)
			g.getParent().removeChild(g, um);

		return groupsToDelete;
	}

	static void cleanBeforeDelete(const ValueTree& vToBeDeleted, UndoManager* um)
	{
		auto root = valuetree::Helpers::findParentWithType(vToBeDeleted, PropertyIds::Network);

		if(vToBeDeleted.getType() == PropertyIds::Node)
		{
			StringArray nodeIds;

			forEach(vToBeDeleted, PropertyIds::Node, [&](ValueTree& v)
			{
				nodeIds.add(v[PropertyIds::ID]);
			});

			Array<ValueTree> connectionsToBeRemoved;

			forEach(root, PropertyIds::Connection, [&](ValueTree& c)
			{
				if (nodeIds.contains(c[PropertyIds::NodeId].toString()))
					connectionsToBeRemoved.add(c);
			});

			for(auto& c: connectionsToBeRemoved)
			{
				cleanBeforeDelete(c, um);
				c.getParent().removeChild(c, um);
			}

			removeFromGroups(root, nodeIds, um);
		}
		else if (vToBeDeleted.getType() == PropertyIds::Connection)
		{
			auto path = ParameterHelpers::getParameterPath(vToBeDeleted);

			if(ParameterHelpers::isNodeConnection(vToBeDeleted))
			{
				path = path.upToLastOccurrenceOf(".", false, false);

				forEach(root, PropertyIds::Node, [&](ValueTree& n)
				{
					if (n[PropertyIds::ID].toString() == path)
						n.removeProperty(PropertyIds::Automated, um);
				});

			}
			else
			{
				forEach(root, PropertyIds::Parameter, [&](ValueTree& p)
				{
					if (ParameterHelpers::getParameterPath(p) == path)
						p.removeProperty(PropertyIds::Automated, um);
				});
			}

			
		}
	}

	static void updateIdsRecursive(const ValueTree& rootNode, ValueTree& vToUpdate, std::map<String, String>& changes)
	{
		jassert(rootNode.getType() == PropertyIds::Network);
		jassert(vToUpdate.getType() == PropertyIds::Node);

		auto thisId = vToUpdate[PropertyIds::ID];
		auto newId = Helpers::getUniqueId(thisId, rootNode);

		changes[thisId] = newId;

		if(thisId != newId)
			vToUpdate.setProperty(PropertyIds::ID, newId, nullptr);

		for(auto cn: vToUpdate.getChildWithName(PropertyIds::Nodes))
			updateIdsRecursive(rootNode, cn, changes);
	}

	static void updateIds(const ValueTree& rootNode, Array<ValueTree>& nodesToUpdate)
	{
		jassert(rootNode.getType() == PropertyIds::Network);

		for(auto& vToUpdate: nodesToUpdate)
		{
			jassert(vToUpdate.getType() == PropertyIds::Node);
			jassert(!vToUpdate.isAChildOf(rootNode));
		}

		std::map<String, String> changes;

		StringArray internalParameterConnections;
		Array<ValueTree> connectionsToRemove;

		for(auto& vToUpdate: nodesToUpdate)
		{
			updateIdsRecursive(rootNode, vToUpdate, changes);
		}

		for(auto& vToUpdate: nodesToUpdate)
		{
			forEach(vToUpdate, PropertyIds::Connection, [&](ValueTree& c)
			{
				auto n = c[PropertyIds::NodeId].toString();

				if (changes.find(n) != changes.end())
				{
					c.setProperty(PropertyIds::NodeId, changes.at(n), nullptr);
					internalParameterConnections.add(ParameterHelpers::getParameterPath(c));
				}
				else
					connectionsToRemove.add(c);
			});
		}

		for(auto c: connectionsToRemove)
			c.getParent().removeChild(c, nullptr);

		for(auto& vToUpdate: nodesToUpdate)
		{
			forEach(vToUpdate, PropertyIds::Parameter, [&](ValueTree& p)
			{
				if (p[PropertyIds::Automated])
				{
					if (!internalParameterConnections.contains(ParameterHelpers::getParameterPath(p)))
						p.removeProperty(PropertyIds::Automated, nullptr);
				}
			});
		}
	}

	static ValueTree createNode(const NodeDatabase& db, const String& factoryPath, const CreateData& cd, UndoManager* um)
	{
		jassert(cd.source != nullptr || cd.signalIndex != -1);

		Colour c;

		auto v = db.getValueTree(factoryPath);

		if(!v.isValid())
			return v;

		if(cd.source != nullptr)
			c = ParameterHelpers::getParameterColour(cd.source->data);

		auto rt = valuetree::Helpers::findParentWithType(cd.containerToInsert, PropertyIds::Network);

		auto newId = Helpers::getUniqueId(factoryPath.fromFirstOccurrenceOf(".", false, false), rt);

		v.setProperty(PropertyIds::ID, newId, nullptr);
		v.setProperty(PropertyIds::NodeColour, c.getARGB(), nullptr);

		auto yOffset = Helpers::HeaderHeight;
		yOffset += Helpers::ParameterMargin;
		yOffset += Helpers::ParameterHeight / 2;

		v.setProperty(UIPropertyIds::x, cd.pointInContainer.getX(), nullptr);
		v.setProperty(UIPropertyIds::y, cd.pointInContainer.getY() - yOffset, nullptr);

		if(cd.target != nullptr)
			v.setProperty(PropertyIds::Folded, true, nullptr);

		Helpers::migrateFeedbackConnections(v, true, nullptr);

		auto nodeTree = cd.containerToInsert.getChildWithName(PropertyIds::Nodes);

		Array<ValueTree> list;
		list.add(v);

		updateIds(rt, list);

		nodeTree.addChild(v, cd.signalIndex, um);

		if(cd.source != nullptr)
		{
			auto conTree = cd.source->getConnectionTree();

			ValueTree nc(PropertyIds::Connection);
			nc.setProperty(PropertyIds::NodeId, newId, nullptr);
			nc.setProperty(PropertyIds::ParameterId, "Value", nullptr);
			conTree.addChild(nc, -1, um);

			auto ptree = v.getOrCreateChildWithName(PropertyIds::Parameters, nullptr);
			ptree.getChildWithProperty(PropertyIds::ID, "Value").setProperty(PropertyIds::Automated, true, nullptr);
		}

		if(cd.target != nullptr)
		{
			auto con = ParameterHelpers::getConnection(cd.target->data);

			jassert(con.isValid());
			con.getParent().removeChild(con, um);
			con.removeProperty(UIPropertyIds::CableOffset, um);

			auto mt = v.getChildWithName(PropertyIds::ModulationTargets);

			if(!mt.isValid())
				mt = v.getChildWithName(PropertyIds::SwitchTargets).getChild(0);

			auto nb = Helpers::getBounds(v, false);
			

			Helpers::translatePosition(cd.target->getNodeTree(), { nb.getWidth() + Helpers::NodeMargin, 0 }, um);

			jassert(mt.isValid());
			mt.addChild(con, -1, um);
		}

		auto rootContainer = Helpers::getCurrentRoot(cd.containerToInsert);

		Helpers::updateChannelCount(valuetree::Helpers::getRoot(nodeTree), false, um);

		if(cd.source == nullptr)
		{
			MessageManager::callAsync([rootContainer, um]()
			{
				Helpers::fixOverlap(rootContainer, um, false);
			});
		}
		
		return v;
	}
};

}