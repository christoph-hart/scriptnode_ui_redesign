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

struct ProcessNodeComponent : public NodeComponent
{
	static constexpr int PinSize = 8;

	struct RoutableSignalComponent : public CablePinBase
	{
		RoutableSignalComponent(ProcessNodeComponent& parent_) :
			CablePinBase(parent_.getValueTree(), parent_.um),
			parent(parent_)
		{
			if(!canBeTarget())
				setEnableDragging(true);

			setSize(Helpers::ParameterWidth, Helpers::SignalHeight);
		}
		;

		Helpers::ConnectionType getConnectionType() const override
		{
			return Helpers::ConnectionType::RoutableSignal;
		};

		bool matchesConnectionType(Helpers::ConnectionType ct) const override
		{
			return ct == Helpers::ConnectionType::RoutableSignal;
		};

		bool canBeTarget() const override { return Helpers::getFactoryPath(parent.getValueTree()).second == "receive"; }

		ValueTree getConnectionTree() const override 
		{
			return parent.getValueTree().getChildWithName(PropertyIds::ReceiveTargets);
		}

		void paint(Graphics& g) override
		{
			g.setColour(Colours::grey);

			for (auto& s: screws)
				g.fillPath(s);

			if(!canBeTarget())
			{
				g.setColour(Colours::white.withAlpha(isMouseOver() ? 0.7f : 0.5f));
				g.fillPath(targetIcon);
			}
				
			g.setColour(Colours::white.withAlpha(0.3f));

			auto j = canBeTarget() ? Justification::right : Justification::left;

			g.setFont(GLOBAL_FONT());
			g.drawText(getSourceDescription().fromFirstOccurrenceOf(".", false, false), getLocalBounds().reduced(10, 0).toFloat(), j);
		}

		String getSourceDescription() const override
		{
			String s;

			s << Helpers::getHeaderTitle(getNodeTree()) << ".";

			s << (canBeTarget() ? "Feedback Input" : "Feedback Output");

			return s;
		}

		void resized() override
		{
			if(!canBeTarget())
			{
				auto b = getLocalBounds().toFloat();
				auto pb = b.removeFromRight(getHeight());
				parent.scalePath(targetIcon, pb.reduced(12.0f));
			}

			screws.clear();


			auto b = getLocalBounds().toFloat();

			auto paddingX = JUCE_LIVE_CONSTANT_OFF(8);
			auto paddingY = JUCE_LIVE_CONSTANT_OFF(6);

			if (canBeTarget())
			{
				// receive
				b.removeFromLeft(paddingX);
				b = b.removeFromLeft(PinSize);
			}
			else
			{
				paddingX += JUCE_LIVE_CONSTANT_OFF(25);

				// send
				b.removeFromRight(paddingX);
				b = b.removeFromRight(PinSize);
			}

			b = b.reduced(0, paddingY);

			auto delta = b.getHeight() / (parent.numChannels);

			for (int i = 0; i < parent.numChannels; i++)
			{
				auto s = Helpers::createPinHole();

				parent.scalePath(s, b.removeFromTop(delta).withSizeKeepingCentre(PinSize, PinSize));
				screws.push_back(s);
			}
		}

		String getTargetParameterId() const override {
			return "FeedbackInput";
		}

		std::vector<Path> screws;

		ProcessNodeComponent& parent;
		
	};

	ProcessNodeComponent(Lasso* l, const ValueTree& v, UndoManager* um_) :
		NodeComponent(l, v, um_, true)
	{
		if (Helpers::hasRoutableSignal(v))
			addAndMakeVisible(routableSignal = new RoutableSignalComponent(*this));

		setOpaque(true);

		dummyBody = DummyBody::createDummyComponent(v[PropertyIds::FactoryPath].toString());

		if(routableSignal != nullptr)
		{
			setFixSize({});
		}
		else if (dummyBody != nullptr)
		{
			addAndMakeVisible(dummyBody);
			setFixSize(dummyBody->getLocalBounds());
		}
		else
			setFixSize({});
	}

	void paint(Graphics& g) override
	{
		g.fillAll(getValueTree()[PropertyIds::Folded] ? Colour(0xFF292929) : Colour(0xFF353535));
		g.setColour(Helpers::getNodeColour(getValueTree()));
		g.drawRect(getLocalBounds().toFloat(), 1.0f);

		auto b = getLocalBounds().toFloat();

		g.setColour(Colours::black.withAlpha(0.05f));

		b.removeFromTop(Helpers::HeaderHeight);

		b = b.removeFromTop(5.0f);

		for (int i = 0; i < 5; i++)
		{
			g.fillRect(b);
			b.removeFromBottom(1.0f);
		}

		auto cl = Helpers::getNodeColour(getValueTree());

		if(routableSignal != nullptr)
		{
			jassert(routableSignal->screws.size() == numChannels);

			for(int i = 0; i < numChannels; i++)
			{
				Point<float> p1 = getLocalPoint(routableSignal, routableSignal->screws[i].getBounds().getCentre());
				Point<float> p2;

				float offset = 0.0f;

				if(routableSignal->canBeTarget())
				{
					p2 = cables[i].second.getBounds().getCentre();
					offset = -40.0f;
				}
				else
				{
					p2 = cables[i].first.getBounds().getCentre();
					std::swap(p2, p1);
					offset = 35.0f;
				}
					
				offset += 6.0f * ((float)i / (float)numChannels);

				Path p;
				Helpers::createCustomizableCurve(p, p1, p2, offset, 3.0f, true);

				g.setColour(Colours::black.withAlpha(0.7f));
				g.strokePath(p, PathStrokeType(3.0f));
				g.setColour(cl);
				g.strokePath(p, PathStrokeType(1.0f));
			}
		}

		for(auto& c: cables)
		{
			g.setColour(Colours::grey);
			g.fillPath(c.first);
			g.fillPath(c.second);

			g.setColour(Colours::black.withAlpha(0.7f));
			g.drawLine({ c.first.getBounds().getCentre(), c.second.getBounds().getCentre() }, 3.0f);
			g.setColour(cl);
			g.drawLine({ c.first.getBounds().getCentre(), c.second.getBounds().getCentre() }, 1.0f);
		}
	}

	void onFold(const Identifier& id, const var& newValue) override
	{
		if (dummyBody != nullptr)
			dummyBody->setVisible(!(bool)newValue);

		NodeComponent::onFold(id, newValue);
	}


	void resized() override
	{
		NodeComponent::resized();

		auto b = getLocalBounds();
		b.removeFromTop(Helpers::HeaderHeight);
		b.removeFromTop(Helpers::SignalHeight);

		auto deltaY = 0.0;

		if (routableSignal != nullptr)
		{
			routableSignal->setBounds(b.removeFromTop(Helpers::SignalHeight));
			deltaY += Helpers::SignalHeight;
		}

		if (dummyBody != nullptr && dummyBody->isVisible())
		{
			dummyBody->setTopLeftPosition(b.getTopLeft());

			b.removeFromTop(dummyBody->getHeight());
			deltaY += dummyBody->getHeight();
		}

		for (auto p : parameters)
		{
			p->setTopLeftPosition(p->getPosition().translated(0, deltaY));
		}

		for (auto m : modOutputs)
		{
			m->setTopLeftPosition(m->getPosition().translated(0, deltaY));
		}

		cables.clear();

		b = getLocalBounds();
		b.removeFromTop(Helpers::HeaderHeight);
		auto sb = b.removeFromTop(Helpers::SignalHeight);

		numChannels = Helpers::getNumChannels(getValueTree());

		auto delta = sb.getHeight() / (numChannels + 1);
		sb = sb.reduced(delta * 0.5f);

		for(int i = 0; i < numChannels; i++)
		{
			auto row = sb.removeFromTop(delta);	

			auto p1 = Helpers::createPinHole();
			auto p2 = Helpers::createPinHole();

			PathFactory::scalePath(p1, row.removeFromLeft(12).toFloat().withSizeKeepingCentre(PinSize, PinSize));
			PathFactory::scalePath(p2, row.removeFromRight(12).toFloat().withSizeKeepingCentre(PinSize, PinSize));

			cables.push_back( { p1, p2 });
		}
	}

	std::vector<std::pair<Path, Path>> cables;
	
	ScopedPointer<RoutableSignalComponent> routableSignal;
	ScopedPointer<Component> dummyBody;
	int numChannels = 2;
};

struct LockedContainerComponent: public ProcessNodeComponent,
								 public ContainerComponentBase
{
	LockedContainerComponent(Lasso* l, const ValueTree& v, UndoManager* um_);;

	void paint(Graphics& g) override
	{
		ProcessNodeComponent::paint(g);
		drawOutsideLabel(g);
	}

	CablePinBase* createOutsideParameterComponent(const ValueTree& source) override
	{
		auto target = ParameterHelpers::getTarget(source);
		return new LockedTarget(target, asNodeComponent().getUndoManager());
	}

	void onOutsideParameterChange(int before, int now) override
	{
		// make sure to set the component position before this call to ensure
		// that it ends up at the right place...
		setTopLeftPosition(Helpers::getPosition(getValueTree()));

		asNodeComponent().setFixSize(Rectangle<int>(0, 0, Helpers::SignalHeight + Helpers::ParameterWidth, now * Helpers::ModulationOutputHeight + 20 + Helpers::ParameterMargin * 2));

		ContainerComponentBase::onOutsideParameterChange(before, now);
	}

	Component* createPopupComponent() override;

	void resized() override
	{
		ProcessNodeComponent::resized();

		auto hb = header.getLocalBounds();
		hb.removeFromRight(hb.getHeight());
		gotoButton.setBounds(hb.removeFromRight(hb.getHeight()).reduced(3));

		auto yOffset = Helpers::HeaderHeight + Helpers::SignalHeight + Helpers::ParameterMargin;

		if (auto lp = parameters.getLast())
			yOffset = lp->getBottom();

		positionOutsideParameters(yOffset);
	}

	HiseShapeButton gotoButton;
};

struct NoProcessNodeComponent : public NodeComponent
{
	NoProcessNodeComponent(Lasso* l, const ValueTree& v, UndoManager* um_) :
		NodeComponent(l, v, um_, false)
	{
		setOpaque(true);

		dummyBody = DummyBody::createDummyComponent(v[PropertyIds::FactoryPath].toString());

		if (dummyBody != nullptr)
		{
			addAndMakeVisible(dummyBody);
			setFixSize(dummyBody->getLocalBounds());
		}
		else
			setFixSize({});
	}

	void paint(Graphics& g) override
	{
		g.fillAll(Colour(0xFF353535));
		g.setColour(Helpers::getNodeColour(getValueTree()));
		g.drawRect(getLocalBounds().toFloat(), 1.0f);

		auto b = getLocalBounds().toFloat();

		g.setColour(Colours::black.withAlpha(0.05f));

		b.removeFromTop(Helpers::HeaderHeight);

		b = b.removeFromTop(5.0f);

		for (int i = 0; i < 5; i++)
		{
			g.fillRect(b);
			b.removeFromBottom(1.0f);
		}
	}

	void onFold(const Identifier& id, const var& newValue) override
	{
		if (dummyBody != nullptr)
			dummyBody->setVisible(!(bool)newValue);

		NodeComponent::onFold(id, newValue);
	}

	void resized() override
	{
		NodeComponent::resized();

		if (dummyBody != nullptr && dummyBody->isVisible())
		{
			auto b = getLocalBounds();

			b.removeFromTop(Helpers::HeaderHeight);

			if (hasSignal)
				b.removeFromTop(Helpers::SignalHeight);

			dummyBody->setTopLeftPosition(b.getTopLeft());

			b.removeFromTop(dummyBody->getHeight());

			for (auto p : parameters)
			{
				p->setTopLeftPosition(p->getPosition().translated(0, dummyBody->getHeight()));
			}

			for (auto m : modOutputs)
			{
				m->setTopLeftPosition(m->getPosition().translated(0, dummyBody->getHeight()));
			}

		}
	}

	ScopedPointer<Component> dummyBody;
};

}