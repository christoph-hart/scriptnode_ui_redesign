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


struct CableBase : public Component,
				   public ChangeListener
{
	CableBase(SelectableComponent::Lasso* lassoToSync_);

	virtual ~CableBase();

	bool hitTest(int x, int y) override;
	void changeListenerCallback(ChangeBroadcaster*) override;
	void rebuildPath(Point<float> newStart, Point<float> newEnd, Component* parent);
	void paint(Graphics& g) override;

	Path p;
	Path arrow;
	bool over = false;
	Point<float> s, e;

	float offset = 0.0f;
	float width = 0.333f;

	Colour colour1;
	Colour colour2;

	WeakReference<SelectableComponent::Lasso> lassoToSync;
};

struct DraggedCable : public CableBase
{
	DraggedCable(CablePinBase* src_);
	~DraggedCable();

	void paint(Graphics& g) override;
	void setTargetPosition(Point<int> rootPosition);

	String label;
	bool ok = true;

	CablePinBase::WeakPtr src;
	CablePinBase::WeakPtr hoveredPin;
	Component::SafePointer<ContainerComponent::AddButton> hoveredAddButton;
};

struct CableComponent : public CableBase,
						public SelectableComponent,
						public PooledUIUpdater::SimpleTimer
{
	struct CableLabel : public Component,
						public ComponentMovementWatcher,
						public AsyncUpdater
	{
		CableLabel(CableComponent* c);;

		void handleAsyncUpdate() override
		{
			updatePosition();
		}

		void updatePosition();
		void componentVisibilityChanged() override;
		void componentMovedOrResized(bool wasMoved, bool wasResized) override
		{
			triggerAsyncUpdate();
		}

		void componentPeerChanged() override {};
		void paint(Graphics& g) override;

		Component::SafePointer<CableComponent> attachedCable;
		String currentText;
	};

	struct CablePopup: public Component,
					   public PooledUIUpdater::SimpleTimer
	{
		CablePopup(PooledUIUpdater* updater, CablePinBase* src_, CablePinBase* dst_):
		  SimpleTimer(updater),
		  src(src_),
		  dst(dst_),
		  debugBuffer(new SimpleRingBuffer())
		{
			debugBuffer->setGlobalUIUpdater(updater);
			debugBuffer->setRingBufferSize(1, 44100);
			debugBuffer->setSamplerate(44100.0 * 0.25);

			auto c = ParameterHelpers::getParameterColour(dst->data);

			setColour(complex_ui_laf::NodeColourId, c);

			plotter = debugBuffer->getPropertyObject()->createComponent();
			plotter->setComplexDataUIBase(debugBuffer.get());
			plotter->setSpecialLookAndFeel(&laf, false);
			addAndMakeVisible(dynamic_cast<Component*>(plotter.get()));
			setName("Connection details");
			setSize(Helpers::ParameterWidth * 2 + 256, 100);
			start();
		}

		void timerCallback() override
		{
			auto dTree = dst->data;
			auto pIndex = dTree.getParent().indexOf(dTree);
			auto pSource = dynamic_cast<Component*>(dst.get())->findParentComponentOfClass<ParameterSourceObject>();
			auto rng = pSource->getParameterRange(pIndex);
			auto value = rng.convertTo0to1(pSource->getParameterValue(pIndex), false);
			auto now = Time::getMillisecondCounter();
			auto delta = jmin<int>(30, now - lastTime);

			lastTime = now;

			auto numSamples = roundToInt(44100.0 * 0.25 * (double)delta * 0.001);

			debugBuffer->write(value, numSamples);
		}

		uint32 lastTime = 0;

		void resized() override
		{
			auto pl = getChildComponent(0);
			
			auto b = getLocalBounds();

			sourceArea = b.removeFromLeft(Helpers::ParameterWidth).toFloat();
			targetArea = b.removeFromRight(Helpers::ParameterWidth).toFloat();
			
			pl->setBounds(b);
		}

		void paint(Graphics& g) override
		{
			auto sTree = src->data;
			auto dTree = dst->data;

			if(sTree.getType() == PropertyIds::Parameter)
			{
				draw(g, sTree, sourceArea, src);
			}
			
			draw(g, dTree, targetArea, dst);
		}

		void draw(Graphics& g, const ValueTree& v, Rectangle<float> area, CablePinBase* pin)
		{
			auto c = ParameterHelpers::getParameterColour(v);

			auto index = v.getParent().indexOf(v);
			auto p = pin->findParentComponentOfClass<ParameterSourceObject>();
			auto rng = p->getParameterRange(index);
			auto value = p->getParameterValue(index);

			auto vtc = ValueToTextConverter::fromString(v[PropertyIds::TextToValueConverter].toString());

			auto inverted = (bool)v[PropertyIds::Inverted];

			g.setColour(c);
			g.setFont(GLOBAL_FONT());

			auto low = rng.convertFrom0to1(0.0, inverted);
			auto mid = rng.convertFrom0to1(0.5, inverted);
			auto high = rng.convertFrom0to1(1.0, inverted);

			g.drawText(vtc.getTextForValue(high), area, Justification::centredTop);
			g.drawText(vtc.getTextForValue(mid), area, Justification::centred);
			g.drawText(vtc.getTextForValue(low), area, Justification::centredBottom);
		}

		Rectangle<float> sourceArea, targetArea;

		CablePinBase::WeakPtr src, dst;

		hise::SimpleRingBuffer::Ptr debugBuffer;
		ScopedPointer<RingBufferComponentBase> plotter;

		complex_ui_laf laf;
	};

	struct CableHolder
	{
		CableHolder(const ValueTree& v);

		virtual ~CableHolder() = default;

		void rebuildCables();

		void onHideCable(const ValueTree& v, const Identifier& id)
		{
			rebuildCables();
		}

		void onConnectionChange(const ValueTree& v, bool wasAdded)
		{
			rebuildCables();
		}

		struct Stub;

		struct Blinker : public Timer
		{
			Blinker(CableHolder& p);;

			void blink(WeakReference<CablePinBase> target);
			void unblink(CablePinBase::WeakPtr target);
			void timerCallback() override;

			Array<std::pair<CablePinBase::WeakPtr, int>> blinkTargets;
			CableHolder& parent;
		} blinker;

		OwnedArray<CableComponent> cables;
		OwnedArray<CableLabel> labels;
		OwnedArray<Component> stubs;
		ScopedPointer<DraggedCable> currentlyDraggedCable;

		bool initialised = false;
		valuetree::RecursivePropertyListener hideConnectionListener;
		valuetree::RecursiveTypedChildListener connectionListener;
	};

	static ValueTree getConnectionTree(CablePinBase* src, CablePinBase* dst);

	CableComponent(Lasso& l, CablePinBase* src_, CablePinBase* dst_);

	void onCableOffset(const Identifier&, const var& newValue);
	void updatePosition(const Identifier& id, const var&);

	String getLabelText() const;

	ValueTree getValueTree() const override;
	UndoManager* getUndoManager() const override { return src->um; }
	InvertableParameterRange getParameterRange(int index) const override { jassertfalse; return {}; }
	double getParameterValue(int index) const override { jassertfalse; return {}; }

	void timerCallback() override
	{
		auto lastAlpha = changeAlpha;

		if (dst != nullptr)
		{
			if (auto p = dst->findParentComponentOfClass<ParameterSourceObject>())
			{
				auto pIndex = dst->data.getParent().indexOf(dst->data);

				if(lastValue.setModValueIfChanged(p->getParameterValue(pIndex)))
				{
					if(changeAlpha == -1)
						changeAlpha = 0;
					else
						changeAlpha = 11;
				}
			}
		}

		if(changeAlpha > 0)
		{
			changeAlpha--;

			if(changeAlpha != lastAlpha)
				repaint();
			else
			{
				int x = 5;
			}
		}
	}

	void paintOverChildren(Graphics& g) override;

	void mouseDrag(const MouseEvent& ev) override;
	void mouseEnter(const MouseEvent& ev);
	void mouseDown(const MouseEvent& e) override;
	void mouseMove(const MouseEvent& e) override;
	void mouseExit(const MouseEvent& e) override;

	float downOffset = 0.0f;

	ModValue lastValue;
	int changeAlpha = -1;

	ValueTree connectionTree;
	Point<float> hoverPoint;
	CablePinBase::WeakPtr src, dst;

	valuetree::PropertyListener sourceListener;
	valuetree::PropertyListener targetListener;
	valuetree::PropertyListener cableOffsetListener;

	valuetree::RemoveListener removeListener;
};

}