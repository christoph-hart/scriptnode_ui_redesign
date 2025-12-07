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

namespace scriptnode {
using namespace hise;
using namespace juce;



RangePresets::RangePresets() :
	fileToLoad(getRangePresetFile())
{
	auto xml = XmlDocument::parse(fileToLoad);

	if (xml != nullptr)
	{
		auto v = ValueTree::fromXml(*xml);

		int index = 1;

		for (auto c : v)
		{
			Preset p;
			p.restoreFromValueTree(c);

			p.index = index++;

			presets.add(p);
		}
	}
	else
	{
		createDefaultRange("0-1", { 0.0, 1.0 }, 0.5);
		createDefaultRange("Inverted 0-1", InvertableParameterRange().inverted(), -1.0);
		createDefaultRange("Decibel Gain", { -100.0, 0.0, 0.1 }, -12.0);
		createDefaultRange("1-16 steps", { 1.0, 16.0, 1.0 });
		createDefaultRange("Osc LFO", { 0.0, 10.0, 0.0, 1.0 });
		createDefaultRange("Osc Freq", { 20.0, 20000.0, 0.0 }, 1000.0);
		createDefaultRange("Linear 0-20k Hz", { 0.0, 20000.0, 0.0 });
		createDefaultRange("Freq Ratio Harmonics", { 1.0, 16.0, 1.0 });
		createDefaultRange("Freq Ratio Detune Coarse", { 0.5, 2.0, 0.0 }, 1.0);
		createDefaultRange("Freq Ratio Detune Fine", { 1.0 / 1.1, 1.1, 0.0 }, 1.0);

		ValueTree v("Ranges");

		for (const auto& p : presets)
			v.addChild(p.exportAsValueTree(), -1, nullptr);

		auto xml = v.createXml();

		if(fileToLoad.existsAsFile())		
			fileToLoad.replaceWithText(xml->createDocument(""));
	}
}

juce::File RangePresets::getRangePresetFile()
{
	return File();//ProjectHandler::getAppDataDirectory(nullptr).getChildFile("RangePresets").withFileExtension("xml");
}

void RangePresets::createDefaultRange(const String& id, InvertableParameterRange d, double midPoint /*= -10000000.0*/)
{
	Preset p;
	p.id = id;
	p.nr = d;

	p.index = presets.size() + 1;

	if (d.getRange().contains(midPoint))
		p.nr.setSkewForCentre(midPoint);

	presets.add(p);
}

RangePresets::~RangePresets()
{

}

void RangePresets::Preset::restoreFromValueTree(const ValueTree& v)
{
	nr = RangeHelpers::getDoubleRange(v);
	id = v[PropertyIds::ID].toString();
}

juce::ValueTree RangePresets::Preset::exportAsValueTree() const
{
	ValueTree v("Range");
	v.setProperty(PropertyIds::ID, id, nullptr);
	RangeHelpers::storeDoubleRange(v, nr, nullptr);
	return v;
}


scriptnode::CablePinBase::Connections CablePinBase::rebuildConnections(Component* parent)
{
	Connections connections;

	if (findParentComponentOfClass<NodeComponent::PopupComponent>())
		return connections;

	auto thisType = getConnectionType();

	Component::callRecursive<CablePinBase>(parent, [&](CablePinBase* c)
		{
			if (c->findParentComponentOfClass<NodeComponent::PopupComponent>())
				return false;

			if (c->canBeTarget() && c->matchesConnectionType(thisType))
			{
				auto p1 = c->getTargetParameterId();
				auto n1 = c->getNodeTree()[PropertyIds::ID].toString();

				for (auto d : getConnectionTree())
				{
					if (c->matchConnection(d))
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

void CablePinBase::mouseDown(const MouseEvent& e)
{
	if (e.mods.isRightButtonDown())
		return;

	if (draggingEnabled)
	{
		auto root = findParentComponentOfClass<DspNetworkComponent>();

		root->currentlyDraggedCable = new DraggedCable(this);
		root->addAndMakeVisible(root->currentlyDraggedCable);
	}
}

void CablePinBase::mouseDrag(const MouseEvent& e)
{
	ZoomableViewport::checkDragScroll(e, false);

	if (e.mods.isRightButtonDown())
		return;

	if (draggingEnabled)
	{
		

		auto root = findParentComponentOfClass<DspNetworkComponent>();

		auto rootPos = root->getLocalPoint(this, e.getPosition());
		root->currentlyDraggedCable->setTargetPosition(rootPos);
	}
}

void CablePinBase::mouseUp(const MouseEvent& e)
{
	ZoomableViewport::checkDragScroll(e, true);


	if (draggingEnabled)
	{
		

		auto root = findParentComponentOfClass<DspNetworkComponent>();

		if(root->currentlyDraggedCable == nullptr)
			return;

		if(auto dst = root->currentlyDraggedCable->hoveredPin)
		{
			addConnection(dst);
		}

		root->currentlyDraggedCable = nullptr;
	}
}

void CablePinBase::setEnableDragging(bool shouldBeDragable)
{
	draggingEnabled = shouldBeDragable;
	setMouseCursor(draggingEnabled ? MouseCursor::CrosshairCursor : MouseCursor::NormalCursor);
}

void CablePinBase::addConnection(const WeakPtr dst)
{
	auto sameParentOrDirectChild = getNodeTree().getParent() == dst->getNodeTree().getParent() ||
								   getNodeTree().getParent() == dst->getNodeTree().getParent().getParent().getParent();

	auto parameterId = dst->getTargetParameterId();
	auto nodeId = dst->getNodeTree()[PropertyIds::ID].toString();

	dst->data.setProperty(PropertyIds::Automated, true, um);

	ValueTree c(PropertyIds::Connection);

	c.setProperty(PropertyIds::NodeId, nodeId, nullptr);
	c.setProperty(PropertyIds::ParameterId, parameterId, nullptr);
	c.setProperty(UIPropertyIds::HideCable, !sameParentOrDirectChild, nullptr);

	auto connections = getConnectionTree();
	connections.addChild(c, -1, um);
}

bool ParameterComponent::isOutsideParameter() const
{
	auto p = findParentComponentOfClass<NodeComponent>();
	return p != nullptr && p->getValueTree().getChildWithName(PropertyIds::Parameters) != data.getParent();
}



struct ParameterComponent::RangeComponent : public ComponentWithMiddleMouseDrag,
	public Timer,
	public TextEditor::Listener
{
	enum MousePosition
	{
		Outside,
		Inside,
		Left,
		Right,
		Nothing
	};

	ParameterComponent& parent;

	RangePresets presets;

	ValueTree connectionSource;

	RangeComponent(bool isTemporary, ParameterComponent& parent_) :
		temporary(isTemporary),
		parent(parent_),
		presets()
	{
		connectionSource = ParameterHelpers::getConnection(getParent().data);

		resetRange = getParentRange();
		currentRange = { 0.0, 1.0 };
		currentRange.rng.skew = resetRange.rng.skew;
		oldRange = resetRange;
		shouldFadeIn = false;
		//startTimer(30);
		//timerCallback();
	}

	bool shouldClose = false;
	bool shouldFadeIn = true;

	float closeAlpha = 0.f;

	bool temporary = false;

	void timerCallback() override
	{
		if (exitTime != 0)
		{
			auto now = Time::getMillisecondCounter();

			auto delta = now - exitTime;

			if (delta > 500)
			{
				shouldClose = true;
				exitTime = 0;
			}

			return;
		}

		if (shouldFadeIn)
		{
			closeAlpha += JUCE_LIVE_CONSTANT_OFF(0.15f);

			if (closeAlpha >= 1.0f)
			{
				closeAlpha = 1.0f;
				stopTimer();
				shouldFadeIn = false;
			}

			setAlpha(closeAlpha);
			getParent().setAlpha(1.0f - closeAlpha);

			return;
		}

		if (shouldClose)
		{
			closeAlpha -= JUCE_LIVE_CONSTANT_OFF(0.15f);

			setAlpha(closeAlpha);
			getParent().setAlpha(1.0f - closeAlpha);

			if (closeAlpha < 0.1f)
			{
				stopTimer();
				close(0);
			}

			return;
		}

		Range<double> tr(0.0, 1.0);

		auto cs = currentRange.rng.start;
		auto ce = currentRange.rng.end;

		auto ts = tr.getStart();
		auto te = tr.getEnd();

		auto coeff = 0.7;

		auto ns = cs * coeff + ts * (1.0 - coeff);
		auto ne = ce * coeff + te * (1.0 - coeff);

		currentRange.rng.start = ns;
		currentRange.rng.end = ne;
		repaint();

		auto tl = te - ts;
		auto cl = currentRange.rng.end - currentRange.rng.start;

		if (hmath::abs(tl - cl) < 0.01)
		{
			currentRange = { 0.0, 1.0 };
			stopTimer();

			if (temporary && !getLocalBounds().contains(getMouseXYRelative()))
			{
				close(300);
			}
		}
	}

	Rectangle<float> getTotalArea()
	{
		auto sf = 1.0f / UnblurryGraphics::getScaleFactorForComponent(this);

		auto b = getLocalBounds().toFloat();

		b.removeFromBottom(18.0f);

		b = b.reduced(3.0f * sf, 0.0f);
		return b;
	}

	Rectangle<float> getSliderArea()
	{
		auto sf = jmin(1.0f / UnblurryGraphics::getScaleFactorForComponent(this), 1.0f);

		auto b = getLocalBounds().toFloat().removeFromBottom(24.0f);

		b = b.reduced(3.0f * sf, 12.0f - 3.0f * sf);

		return b;
	}

	Rectangle<float> getRangeArea(bool clip)
	{
		auto fullRange = NormalisableRange<double>(0.0, 1.0);

		auto sf = 1.0f / UnblurryGraphics::getScaleFactorForComponent(this);

		auto b = getTotalArea().reduced(3.0f * sf);

		auto fr = fullRange.getRange();
		auto cr = currentRange.getRange();

		auto sNormalised = (cr.getStart() - fr.getStart()) / fr.getLength();
		auto eNormalised = (cr.getEnd() - fr.getStart()) / fr.getLength();

		if (clip)
		{
			sNormalised = jlimit(0.0, 1.0, sNormalised);
			eNormalised = jlimit(0.0, 1.0, eNormalised);
		}

		auto inner = b;

		inner.removeFromLeft(b.getWidth() * sNormalised);
		inner.removeFromRight((1.0 - eNormalised) * b.getWidth());

		return inner;
	}

	MousePosition getMousePosition(Point<int> p)
	{
		if (dragPos != Nothing)
			return dragPos;

		if (!getLocalBounds().contains(p))
			return Nothing;

		auto r = getRangeArea(true);
		auto x = p.toFloat().getX();

		if (hmath::abs(x - r.getX()) < 8)
			return Left;
		else if (hmath::abs(x - r.getRight()) < 8)
			return Right;
		else if (r.contains(p.toFloat()))
			return Inside;
		else
			return Outside;
	}



	void close(int delayMilliseconds)
	{
		Component::SafePointer<ParameterComponent> s = &getParent();
		Component::SafePointer<RangeComponent> safeThis(this);

		auto f = [s, safeThis]()
		{
			if (s.getComponent() != nullptr)
			{
				Desktop::getInstance().getAnimator().fadeOut(safeThis.getComponent(), 100);
				//s.getComponent()->currentRangeComponent = nullptr;
				s.getComponent()->setAlpha(1.0f);
				s.getComponent()->resized();
			}
		};

		if (delayMilliseconds == 0)
		{
			MessageManager::callAsync(f);
		}
		else
		{
			shouldFadeIn = false;
			shouldClose = true;
			startTimer(30);
		}
	}




	InvertableParameterRange getParentRange()
	{
		InvertableParameterRange d;

		return RangeHelpers::getDoubleRange(getParent().data);

#if 0
		auto r = getParent().slider.getRange();
		d.rng.start = r.getStart();
		d.rng.end = r.getEnd();
		d.rng.skew = getParent().slider.getSkewFactor();
		d.rng.interval = getParent().slider.getInterval();
		d.inv = RangeHelpers::isInverted(getParent().data);
		return d;
#endif
	}

	void setNewRange(InvertableParameterRange rangeToUse, NotificationType updateRange)
	{
		auto& s = getParent();

		RangeHelpers::storeDoubleRange(s.data, rangeToUse, s.um);

		// We don't store anything in the connection tree anymore
		//if(connectionSource.isValid())
		//	RangeHelpers::storeDoubleRange(connectionSource, rangeToUse, s.node->getUndoManager());

		if (updateRange != dontSendNotification)
			oldRange = rangeToUse;

		repaint();
	}

	void setNewRange(NotificationType updateRange)
	{
		auto& s = getParent();

		auto newStart = oldRange.rng.start + currentRange.rng.start * oldRange.getRange().getLength();
		auto newEnd = oldRange.rng.start + currentRange.rng.end * oldRange.getRange().getLength();

		InvertableParameterRange nr;
		nr.rng.start = newStart;
		nr.rng.end = newEnd;
		nr.rng.interval = s.slider.getInterval();
		nr.rng.skew = skewToUse;
		nr.inv = oldRange.inv;

		setNewRange(nr, updateRange);
	}

	void setNewValue(const MouseEvent& e)
	{
		auto t = getTotalArea();
		auto nv = jlimit(0.0, 1.0, ((double)e.getPosition().getX() - t.getX()) / t.getWidth());

		auto pRange = getParentRange();
		auto v = pRange.convertFrom0to1(nv, true);
		getParent().slider.setValue(v, sendNotification);
		repaint();
	}


	void textEditorReturnKeyPressed(TextEditor& t)
	{
		auto r = getParentRange();

		auto v = getParent().slider.getValueFromText(t.getText());

		r.inv = RangeHelpers::isInverted(getParent().data);
		auto isLeft = currentTextPos == Left;// && !inv) || (Right && inv);

		if (currentTextPos == Inside)
			r.setSkewForCentre(v);
		else if (currentTextPos == Outside)
			getParent().slider.setValue(v, sendNotificationAsync);
		else if (isLeft)
			r.rng.start = v;
		else
			r.rng.end = v;

		setNewRange(r, sendNotification);
		createLabel(Nothing);
	}

	void textEditorEscapeKeyPressed(TextEditor&)
	{
		createLabel(Nothing);
	}

	void textEditorFocusLost(TextEditor&) override
	{
		createLabel(Nothing);
	}

	void paint(Graphics& g) override
	{
		UnblurryGraphics ug(g, *this, true);

		auto sf = 1.0f / UnblurryGraphics::getScaleFactorForComponent(this);

		ScriptnodeComboBoxLookAndFeel::drawScriptnodeDarkBackground(g, getTotalArea(), false);

		auto inner = getRangeArea(true);

		{
			auto copy = getRangeArea(false);
			auto w = copy.getWidth();

			for (int i = 0; i < 3; i++)
			{
				copy.removeFromLeft(w / 4.0f);
				ug.draw1PxVerticalLine(copy.getX(), inner.getY(), inner.getBottom());
			}
		}

		g.setColour(Colours::white.withAlpha(0.05f));
		auto p = getMousePosition(getMouseXYRelative());

		g.setColour(Colours::white.withAlpha(0.3f));

		auto inv = RangeHelpers::isInverted(getParent().data);

		{
			g.saveState();

			auto b = getLocalBounds();
			g.excludeClipRegion(b.removeFromLeft(getTotalArea().getX() + 2.0f));
			g.excludeClipRegion(b.removeFromRight(getTotalArea().getX() + 2.0f));

			auto d = getParentRange();

			Path path;





			Path valuePath;

			{
				auto startValue = (float)d.convertFrom0to1(0.0, false);
				valuePath.startNewSubPath({ 0.0f, inv ? startValue : 1.0f - startValue });
				path.startNewSubPath({ 0.0f, inv ? startValue : 1.0f - startValue });
			}

			auto modValue = getParent().slider.getValue();



			auto valueAssI = d.convertTo0to1(modValue, false);

			if (inv)
				valueAssI = 1.0 - valueAssI;

			//auto valueAssI = d.convertTo0to1(getParent().getValue());

			for (float i = 0.0f; i < 1.0f; i += (0.5f / inner.getWidth()))
			{
				auto v = d.convertFrom0to1(i, false);
				v = d.snapToLegalValue(v);

				path.lineTo(i, inv ? v : 1.0f - v);

				if (i < valueAssI)
					valuePath.lineTo(i, inv ? v : 1.0f - v);
			}

			auto endValue = (float)d.convertFrom0to1(1.0, false);

			valuePath.startNewSubPath(1.0f, inv ? endValue : 1.0f - endValue);
			path.startNewSubPath(1.0f, inv ? endValue : 1.0f - endValue);

			auto pBounds = getRangeArea(false).reduced(2.0f * sf);

			path.scaleToFit(pBounds.getX(), pBounds.getY(), pBounds.getWidth(), pBounds.getHeight(), false);
			valuePath.scaleToFit(pBounds.getX(), pBounds.getY(), pBounds.getWidth(), pBounds.getHeight(), false);

			g.setColour(Colours::white.withAlpha(0.5f));

			auto psf = jmin(2.0f, sf * 1.5f);

			Path dashed;
			float dl[2] = { psf * 2.0f, psf * 2.0f };
			PathStrokeType(2.0f * psf).createDashedStroke(dashed, path, dl, 2);

			g.fillPath(dashed);

			g.setColour(Colour(0xFF262626).withMultipliedBrightness(JUCE_LIVE_CONSTANT_OFF(1.25f)));
			g.strokePath(valuePath, PathStrokeType(psf * JUCE_LIVE_CONSTANT_OFF(4.5f), PathStrokeType::curved, PathStrokeType::rounded));

			g.setColour(Colour(0xFF9099AA).withMultipliedBrightness(JUCE_LIVE_CONSTANT_OFF(1.25f)));
			g.strokePath(valuePath, PathStrokeType(psf * JUCE_LIVE_CONSTANT_OFF(3.0f), PathStrokeType::curved, PathStrokeType::rounded));

			g.restoreState();
		}

		float alpha = 0.5f;

		if (isMouseButtonDown())
			alpha += 0.2f;

		if (isMouseOverOrDragging())
			alpha += 0.1f;

		g.setColour(Colour(SIGNAL_COLOUR).withAlpha(alpha));

		g.setFont(GLOBAL_BOLD_FONT());

		if (!isTimerRunning())
		{
			switch (p)
			{
			case Left:
				g.fillRect(inner.removeFromLeft(4.0f * sf));
				break;
			case Right:
				g.fillRect(inner.removeFromRight(4.0f * sf));
				break;
			case Inside:
			{
				break;
			}
			default: break;
			}
		}

		if (editor == nullptr)
		{
			g.setColour(Colours::white);
			g.drawText(getDragText(p), getLocalBounds().toFloat().removeFromBottom(24.0f), Justification::centred);

			if (p == Outside && !connectionSource.isValid())
			{
				auto b = getSliderArea();

				g.setColour(Colours::white.withAlpha(0.1f));
				g.fillRoundedRectangle(b, b.getHeight() / 2.0f);

				auto nv = getParentRange().convertTo0to1(getParent().slider.getValue(), false);

				if (inv)
					nv = 1.0 - nv;

				auto w = jmax<float>(b.getHeight(), b.getWidth() * nv);

				b = b.removeFromLeft(w);

				g.setColour(Colours::white.withAlpha(isMouseButtonDown(true) ? 0.8f : 0.6f));
				g.fillRoundedRectangle(b, b.getHeight() / 2.0f);
			}
		}
	}

	String getDragText(MousePosition p)
	{
		if (isTimerRunning())
			return {};

		auto newStart = oldRange.rng.start + currentRange.rng.start * oldRange.getRange().getLength();
		auto newEnd = oldRange.rng.start + currentRange.rng.end * oldRange.getRange().getLength();

		switch (p)
		{
		case Left:
		case Right: return getParent().slider.getTextFromValue(newStart) + " - " + getParent().slider.getTextFromValue(newEnd);
		case Inside: return "Mid: " + String(getParentRange().convertFrom0to1(0.5, false));
		default: break;
		}

		return {};
	}

	ParameterComponent& getParent() { return parent; }

	void createLabel(MousePosition p)
	{
		if (p == Nothing)
		{
			MessageManager::callAsync([&]()
				{
					editor = nullptr;
					resized();
				});
		}
		else
		{
			currentTextPos = p;
			addAndMakeVisible(editor = new TextEditor());
			editor->addListener(this);

			String t;

			switch (p)
			{
			case Left:   t = getParent().slider.getTextFromValue(getParent().slider.getMinimum()); break;
			case Right:  t = getParent().slider.getTextFromValue(getParent().slider.getMaximum()); break;
			case Inside: t = String(getParentRange().convertFrom0to1(0.5, false)); break;
			case Outside: t = getParent().slider.getTextFromValue(getParent().slider.getValue()); break;
			default: break;
			}

			editor->setColour(Label::textColourId, Colours::white);
			editor->setColour(Label::backgroundColourId, Colours::transparentBlack);
			editor->setColour(Label::outlineColourId, Colours::transparentBlack);
			editor->setColour(TextEditor::textColourId, Colours::white);

			editor->setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
			editor->setColour(TextEditor::outlineColourId, Colours::transparentBlack);
			editor->setColour(TextEditor::highlightColourId, Colour(SIGNAL_COLOUR).withAlpha(0.5f));
			editor->setColour(TextEditor::ColourIds::focusedOutlineColourId, Colour(SIGNAL_COLOUR));
			editor->setColour(Label::ColourIds::outlineWhenEditingColourId, Colour(SIGNAL_COLOUR));

			editor->setJustification(Justification::centred);
			editor->setFont(GLOBAL_BOLD_FONT());
			editor->setText(t, dontSendNotification);
			editor->selectAll();

			editor->grabKeyboardFocus();

			resized();
		}
	}

	void resized() override
	{
		if (editor != nullptr)
		{
			editor->setBounds(getLocalBounds().removeFromBottom(24));
		}
	}

	void mouseEnter(const MouseEvent& e) override
	{
		if (exitTime != 0)
		{
			if (shouldClose)
			{
				jassert(isTimerRunning());
				shouldClose = false;
				shouldFadeIn = true;
			}

			exitTime = 0;
			stopTimer();
		}
	}

	void mouseExit(const MouseEvent& e) override
	{
		dragPos = Nothing;



		if (temporary && !isTimerRunning())
		{
			exitTime = Time::getMillisecondCounter();
			startTimer(30);
		}


		repaint();
	}



	void mouseDoubleClick(const MouseEvent& e) override
	{
		temporary = !temporary;

		if (!temporary && !e.mods.isAltDown())
		{
			exitTime = Time::getMillisecondCounter();
			startTimer(30);
		}
	}

	void mouseMove(const MouseEvent& e) override
	{
		if (!shouldClose && temporary && !e.mods.isAltDown() && !isTimerRunning())
		{
			close(300);
		}

		switch (getMousePosition(e.getPosition()))
		{
		case Left:	  setMouseCursor(MouseCursor::LeftEdgeResizeCursor); break;
		case Right:   setMouseCursor(MouseCursor::RightEdgeResizeCursor); break;
		case Outside: setMouseCursor(MouseCursor::NormalCursor); break;
		case Inside:  setMouseCursor(MouseCursor::UpDownResizeCursor); break;
		default: break;
		}

		repaint();
	}

	void mouseDown(const MouseEvent& e) override
	{
		if (e.mods.isShiftDown())
		{
			temporary = false;
			createLabel(getMousePosition(e.getPosition()));
			return;
		}

		if (e.mods.isRightButtonDown())
		{
			Component::SafePointer<RangeComponent> rc = this;

			PopupMenu m;
			m.setLookAndFeel(&getParent().laf);

			m.addItem(1, "Make sticky", true, !temporary);

			m.addSeparator();

			PopupMenu ranges;

			constexpr auto ConverterOffset = 5000;
			constexpr auto ModulationOffset = 6000;
			constexpr auto RangeOffset = 9000;

			for (const auto& p : presets.presets)
				ranges.addItem(RangeOffset + p.index, p.id, true, RangeHelpers::isEqual(getParentRange(), p.nr));

			m.addSubMenu("Load Range Preset", ranges);
			m.addItem(3, "Save Range Preset");
			m.addSeparator();
			m.addItem(4, "Reset Range");
			m.addItem(6, "Reset skew", getParent().slider.getSkewFactor() != 1.0);
			m.addSeparator();
			m.addItem(5, "Invert range", true, RangeHelpers::isInverted(getParent().data));
			m.addItem(7, "Copy range to source", connectionSource.isValid());
			m.addItem(8, "Set as default value");

			PopupMenu tc;



			int idx = 0;

			auto currentConverter = getParent().data[PropertyIds::TextToValueConverter].toString();

			for (auto& n : parameter::pod::getTextValueConverterNames())
			{
				auto active = n == currentConverter;

				if (idx == ((int)(parameter::pod::TextValueConverters::numTextValueConverters)))
					break;

				tc.addItem(ConverterOffset + idx++, n, true, active);
			}

			m.addSubMenu("TextConverter", tc);

			auto r = m.show();

			if (rc.getComponent() == 0)
				return;

			if (r == 0)
			{
				if (temporary && !getLocalBounds().contains(getMouseXYRelative()))
					close(300);
			}

			if (r == 1)
			{
				temporary = !temporary;

				if (temporary)
					close(300);
			}
			if (r == 4)
				setNewRange(resetRange, sendNotification);

			if (r == 3)
			{
				jassertfalse;
#if 0
				auto n = PresetHandler::getCustomName("Range");

				if (n.isNotEmpty())
				{
					auto cr = getParentRange();
					presets.createDefaultRange(n, cr);
				}
#endif
			}
			if (r == 5)
			{
				auto cr = getParentRange();
				cr.inv = !RangeHelpers::isInverted(getParent().data);
				setNewRange(cr, sendNotification);
			}
			if (r == 6)
			{
				auto cr = getParentRange();
				cr.rng.skew = 1.0;
				cr.inv = RangeHelpers::isInverted(getParent().data);
				setNewRange(cr, sendNotification);
			}
			if (r == 7)
			{
				auto cr = getParentRange();

				// TODO...
				jassertfalse;
#if 0
				auto parameterTrees = ParameterSlider::getValueTreesForSourceConnection(connectionSource);

				for (auto ptree : parameterTrees)
				{
					RangeHelpers::storeDoubleRange(ptree, cr, getParent().um);
				}
#endif
			}
			if (r == 8)
			{
				auto p = getParent().data;
				p.setProperty(PropertyIds::DefaultValue, p[PropertyIds::Value], getParent().um);
			}

			if (r > RangeOffset)
			{
				auto p = presets.presets[r - 9001];
				setNewRange(p.nr, sendNotification);
			}
			else if (r > ModulationOffset)
			{
				idx = r - ModulationOffset;
				auto n = OpaqueNode::ModulationProperties::getModulationModeNames()[idx];
				auto p = getParent().data;
				p.setProperty(PropertyIds::ExternalModulation, n, getParent().um);
			}
			else if (r > ConverterOffset)
			{
				idx = r - ConverterOffset;
				auto n = parameter::pod::getTextValueConverterNames()[idx];
				auto p = getParent().data;
				p.setProperty(PropertyIds::TextToValueConverter, n, getParent().um);
			}

			repaint();
			return;
		}


		dragPos = getMousePosition(e.getPosition());

		oldRange = getParentRange();

		if (dragPos == Outside)
			setNewValue(e);

		currentRangeAtDragStart = currentRange;

		//currentRangeAtDragStart.rng.skew = getParent().slider.getSkewFactor();

		skewToUse = currentRangeAtDragStart.rng.skew;
		repaint();
	}

	void mouseUp(const MouseEvent& e) override
	{
		if (e.mods.isRightButtonDown() || e.mods.isShiftDown())
			return;

		setNewRange(sendNotification);

		if (dragPos == Left || dragPos == Right)
		{
			shouldClose = false;
			startTimer(30);
		}

		dragPos = Nothing;
		repaint();
	}

	void mouseDrag(const MouseEvent& e) override
	{
		if (e.mods.isRightButtonDown() || e.mods.isShiftDown())
			return;

		if (!e.mouseWasDraggedSinceMouseDown())
			return;

		if (dragPos == Outside)
		{
			if (!connectionSource.isValid())
				setNewValue(e);
		}
		if (dragPos == Inside)
		{
			auto d = (float)e.getDistanceFromDragStartY() / getTotalArea().getHeight();
			auto delta = hmath::pow(2.0f, -1.0f * d);
			auto skewStart = currentRangeAtDragStart.rng.skew;

			skewToUse = jmax(0.001, skewStart * delta);

			currentRange.rng.skew = skewToUse;
			setNewRange(dontSendNotification);
		}
		else if (dragPos == Left || dragPos == Right)
		{
			auto d = (float)e.getDistanceFromDragStartX() / getTotalArea().getWidth();

			if (e.mods.isCommandDown())
			{
				d -= hmath::fmod(d, 0.25f);
			}

			auto r = currentRangeAtDragStart;

			if (dragPos == Left)
			{
				auto v = jmin(currentRangeAtDragStart.rng.end - 0.05, currentRangeAtDragStart.rng.start + d * currentRangeAtDragStart.getRange().getLength());
				currentRange.rng.start = v;
			}
			else
			{
				auto v = jmax(currentRangeAtDragStart.rng.start + 0.05, currentRangeAtDragStart.rng.end + d * currentRangeAtDragStart.getRange().getLength());
				currentRange.rng.end = v;
			}

			setNewRange(dontSendNotification);
		}

		repaint();
	}

	size_t exitTime = 0;

	double skewToUse = 1.0;

	MousePosition dragPos = Nothing;
	InvertableParameterRange currentRangeAtDragStart;
	InvertableParameterRange currentRange = { 0.0, 1.0 };

	InvertableParameterRange oldRange;

	InvertableParameterRange resetRange;

	MousePosition currentTextPos;
	ScopedPointer<TextEditor> editor;
};

void ParameterComponent::mouseDown(const MouseEvent& e)
{
	CablePinBase::mouseDown(e);

	if (e.mods.isRightButtonDown())
	{
		auto nc = new RangeComponent(false, *this);
		nc->setSize(400, 200);
		DspNetworkComponent::showPopup(nc, this);
	}
}

}