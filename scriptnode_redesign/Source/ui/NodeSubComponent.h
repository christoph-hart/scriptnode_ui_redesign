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

    virtual String getTargetParameterId() const = 0;
    virtual ValueTree getConnectionTree() const = 0;

    ValueTree getNodeTree() const 
    {
        if(data.getType() == PropertyIds::Node)
            return data;

        return valuetree::Helpers::findParentWithType(data, PropertyIds::Node);
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

	Connections rebuildConnections(Component* parent)
	{
		Connections connections;

        auto thisType = getConnectionType();

		Component::callRecursive<CablePinBase>(parent, [&](CablePinBase* c)
		{
            if(c->canBeTarget() && c->matchesConnectionType(thisType))
            {
                auto p1 = c->getTargetParameterId();
				auto n1 = c->getNodeTree()[PropertyIds::ID].toString();

				for (auto d : getConnectionTree())
				{
                    if(c->matchConnection(d))
                    {
                        connections.push_back(c);
                        break;
                    }
				}
            }
				
			return false;
		});

		return connections;
	}

	void mouseDown(const MouseEvent& e) override;

	void mouseDrag(const MouseEvent& e) override;

	void mouseUp(const MouseEvent& e) override;

	void setEnableDragging(bool shouldBeDragable);

	void addConnection(const WeakPtr dst);

	ValueTree data;
	UndoManager* um;

protected:

	Path targetIcon;

private:

	bool draggingEnabled = false;

	JUCE_DECLARE_WEAK_REFERENCEABLE(CablePinBase);
};

struct ParameterComponent : public CablePinBase
{
	ParameterComponent(ValueTree v, UndoManager* um_) :
		CablePinBase(v, um_)
	{
		jassert(v.getType() == PropertyIds::Parameter);
		addAndMakeVisible(slider);
		slider.setLookAndFeel(&laf);
		slider.setRange(0.0, 1.0, 0.0);
		slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
		slider.setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, false, 0, 0);

		Colour tr = Colours::transparentBlack;
		slider.setColour(HiseColourScheme::ColourIds::ComponentFillTopColourId, tr);
		slider.setColour(HiseColourScheme::ColourIds::ComponentFillBottomColourId, tr);
		slider.setColour(HiseColourScheme::ColourIds::ComponentOutlineColourId, tr);
		slider.setScrollWheelEnabled(false);

		slider.getValueObject().referTo(v.getPropertyAsValue(PropertyIds::Value, um, false));

		setSize(Helpers::ParameterWidth, Helpers::ParameterHeight);

		if(Helpers::isContainerNode(valuetree::Helpers::findParentWithType(v, PropertyIds::Node)))
		{
			setEnableDragging(true);

			if (!v.hasProperty(PropertyIds::NodeColour))
			{
				auto c = Helpers::getParameterColour(v);
				v.setProperty(PropertyIds::NodeColour, c.getARGB(), um);
			}
		}
	};

	bool isOutsideParameter() const;

	void paint(Graphics& g) override
	{
		if (Helpers::isContainerNode(getNodeTree()))
		{
			g.setColour(Colours::black.withAlpha(0.2f));
			g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(3.0f, 0.0f), 2.0f);

			g.setColour(Helpers::getParameterColour(data));
			g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(3.0f, 0.0f).removeFromRight(4.0f), 2.0f);

			g.setColour(Colours::white.withAlpha(0.5f));
			auto pb = getLocalBounds().removeFromRight(getHeight()).toFloat().reduced(10.0f);
			scalePath(targetIcon, pb);
			g.fillPath(targetIcon);
		}

		auto name = data[PropertyIds::ID].toString();
		
		
		auto tb = getLocalBounds().toFloat();
		tb.removeFromLeft(Helpers::ParameterHeight + 10.0f);

		if(isOutsideParameter())
		{
			g.setFont(GLOBAL_FONT());
			auto rootId = Helpers::getHeaderTitle(valuetree::Helpers::findParentWithType(data, PropertyIds::Node));
			g.setColour(Colours::white.withAlpha(0.4f));
			g.drawText("from " + rootId, tb.removeFromBottom(tb.getHeight() * 0.5), Justification::left);
		}

		g.setFont(GLOBAL_BOLD_FONT());
		g.setColour(Colours::white);
		
		g.drawText(name, tb, Justification::left);
	}

	Helpers::ConnectionType getConnectionType() const override { return Helpers::ConnectionType::Parameter; };

	bool matchesConnectionType(Helpers::ConnectionType ct) const override
	{
		// matches all types except for routable signals (bypass, modulation, parameter)
		return ct != Helpers::ConnectionType::RoutableSignal;
	};

	bool canBeTarget() const override { return true; }

	String getTargetParameterId() const override { return data[PropertyIds::ID].toString(); }

	hise::GlobalHiseLookAndFeel laf;

	ValueTree getConnectionTree() const override
	{
		return data.getChildWithName(PropertyIds::Connections);
	}

	void resized() override
	{
		auto b = getLocalBounds();
		b.removeFromLeft(5);
		slider.setBounds(b.removeFromLeft(Helpers::ParameterHeight));
	}

	juce::Slider slider;
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

		auto b = getLocalBounds().toFloat();
		auto tb = b.removeFromRight(getHeight()).toFloat();

		scalePath(targetIcon, tb.reduced(4.0f));

		g.fillPath(targetIcon);

		if (data.getType() == PropertyIds::ModulationTargets)
		{
			g.drawText("Output", b, Justification::right);
		}
		else
		{
			auto idx = data.getParent().indexOf(data);
			g.drawText("Output " + String(idx + 1), b, Justification::right);
		}
	}

	

	ValueTree getConnectionTree() const override
	{
		auto conTree = data;
		if (conTree.getType() == PropertyIds::SwitchTarget)
			conTree = data.getChildWithName(PropertyIds::Connections);

		return conTree;
	}


};

}