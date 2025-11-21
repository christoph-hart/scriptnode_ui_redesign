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
	struct RoutableSignalComponent : public CablePinBase
	{
		RoutableSignalComponent(ProcessNodeComponent& parent_) :
			CablePinBase(parent_.getValueTree(), parent_.um),
			parent(parent_)
		{};

		Helpers::ConnectionType getConnectionType() const override
		{
			return Helpers::ConnectionType::RoutableSignal;
		};

		bool matchesConnectionType(Helpers::ConnectionType ct) const override
		{
			return ct == Helpers::ConnectionType::RoutableSignal;
		};

		bool canBeTarget() const override { return Helpers::getFactoryPath(parent.getValueTree()).second == "receive"; }

		ValueTree getConnectionTree() const override {
			return parent.getValueTree().getChildWithName(PropertyIds::Properties);
		}

		bool matchConnection(const ValueTree& possibleConnection) const override
		{
			if (possibleConnection[PropertyIds::ID].toString() == PropertyIds::Connection.toString())
			{
				auto connectedTargets = StringArray::fromTokens(possibleConnection[PropertyIds::Value].toString(), ";", "");
				connectedTargets.trim();
				connectedTargets.removeEmptyStrings();

				auto n1 = getNodeTree()[PropertyIds::ID].toString();
				return connectedTargets.contains(n1);
			}

			return false;
		}

		void paint(Graphics& g) override
		{
			auto b = getLocalBounds().toFloat();

			if (canBeTarget())
			{
				// receive
				b = b.removeFromLeft(30.0f);
			}
			else
			{
				// send
				b = b.removeFromRight(30.0f);
			}

			g.setColour(Colours::white);
			g.fillRect(b);
		}

		String getTargetParameterId() const override {
			return "Connection";
		}

		ProcessNodeComponent& parent;
		
	};

	ProcessNodeComponent(Lasso& l, const ValueTree& v, UndoManager* um_) :
		NodeComponent(l, v, um_, true)
	{
		if (Helpers::hasRoutableSignal(v))
			addAndMakeVisible(routableSignal = new RoutableSignalComponent(*this));

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

		auto container = valuetree::Helpers::findParentWithType(getValueTree(), PropertyIds::Node);

		auto cl = Helpers::getNodeColour(container);

		

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

	void resized() override
	{
		NodeComponent::resized();

		if (routableSignal != nullptr)
		{
			auto b = getLocalBounds();
			b.removeFromTop(Helpers::HeaderHeight + Helpers::SignalHeight);
			routableSignal->setBounds(b.removeFromTop(Helpers::SignalHeight));
		}

		if (dummyBody != nullptr)
		{
			auto b = getLocalBounds();

			b.removeFromTop(Helpers::HeaderHeight);

			if (hasSignal)
				b.removeFromTop(Helpers::SignalHeight);

			if (!parameters.isEmpty())
				b.removeFromLeft(Helpers::ParameterMargin + Helpers::ParameterWidth);

			if (!modOutputs.isEmpty())
				b.removeFromRight(Helpers::ParameterWidth);

			dummyBody->setTopLeftPosition(b.getPosition());
		}

		cables.clear();

		auto b = getLocalBounds();
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

			PathFactory::scalePath(p1, row.removeFromLeft(12).toFloat().reduced(1.0f));
			PathFactory::scalePath(p2, row.removeFromRight(12).toFloat().reduced(1.0f));

			cables.push_back( { p1, p2 });
		}
	}

	std::vector<std::pair<Path, Path>> cables;
	
	ScopedPointer<RoutableSignalComponent> routableSignal;
	ScopedPointer<Component> dummyBody;
	int numChannels = 2;
};

struct NoProcessNodeComponent : public NodeComponent
{
	NoProcessNodeComponent(Lasso& l, const ValueTree& v, UndoManager* um_) :
		NodeComponent(l, v, um_, false)
	{
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

	void resized() override
	{
		NodeComponent::resized();

		if (dummyBody != nullptr)
		{
			auto b = getLocalBounds();

			b.removeFromTop(Helpers::HeaderHeight);

			if (hasSignal)
				b.removeFromTop(Helpers::SignalHeight);

			if (!parameters.isEmpty())
				b.removeFromLeft(Helpers::ParameterMargin + Helpers::ParameterWidth);

			if (!modOutputs.isEmpty())
				b.removeFromRight(Helpers::ParameterWidth);

			dummyBody->setTopLeftPosition(b.getPosition());
		}
	}

	ScopedPointer<Component> dummyBody;
};

}