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

void ContainerComponent::CableSetup::updatePins(ContainerComponent& p)
{
	
	pinPositions.containerStart = { 0.0f, (float)Helpers::HeaderHeight };
	pinPositions.containerEnd = pinPositions.containerStart.translated((float)p.getWidth(), 0.0f);
	pinPositions.containerStart = pinPositions.containerStart.translated(Helpers::ParameterMargin + Helpers::ParameterWidth - 25.0f, 0.0f);

	pinPositions.childPositions.clear();

	if(Helpers::isFoldedOrLockedContainer(p.getValueTree()))
	{
		p.repaint();
		return;
	}

	int idx = 0;

	Array<NodeComponent*> processNodes;

	for(auto cn: p.childNodes)
	{
		if (cn == nullptr || cn->getParentComponent() != &parent) // probably dragged
			continue;

		if(Helpers::isProcessingSignal(cn->getValueTree()))
		{
			processNodes.add(cn);
		}
	}

	for(int i = 0; i < processNodes.size(); i++)
	{
		auto cn = processNodes[i];
		

		if (isForcedVertical)
		{
			auto sb = cn->getBoundsInParent().toFloat();

			Point<float> t1(sb.getCentreX(), sb.getY());
			Point<float> t2(sb.getCentreX(), sb.getBottom());

			auto first = i == 0;
			auto last = i == processNodes.size()-1;

			if (first)
				t1 = { sb.getX(), sb.getY() + Helpers::HeaderHeight };

			if (last)
				t2 = { sb.getRight(), sb.getY() + Helpers::HeaderHeight };

			pinPositions.childPositions.push_back({ t1, t2, i });
		}
		else
		{
			Rectangle<float> sb(0.0f, (float)Helpers::HeaderHeight, (float)cn->getWidth(), (float)Helpers::SignalHeight);
			sb = p.getLocalArea(cn, sb);
			pinPositions.childPositions.push_back({ sb.getTopLeft(), sb.getTopRight(), i });
		}
	}

#if 0
	for (auto cn : p.childNodes)
	{
		if (cn->getParentComponent() != &parent) // probably dragged
			continue;

		if (Helpers::isProcessingSignal(cn->getValueTree()))
		{
			if (isForcedVertical)
			{
				auto sb = cn->getBoundsInParent().toFloat();

				Point<float> t1(sb.getCentreX(), sb.getY());
				Point<float> t2(sb.getCentreX(), sb.getBottom());

				auto first = pinPositions.childPositions.empty();
				auto last = p.childNodes.getLast() == cn;

				if (first)
					t1 = { sb.getX(), sb.getY() + Helpers::HeaderHeight };

				if (last)
					t2 = { sb.getRight(), sb.getY() + Helpers::HeaderHeight };

				pinPositions.childPositions.push_back({ t1, t2, idx });

				
			}
			else
			{
				Rectangle<float> sb(0.0f, (float)Helpers::HeaderHeight, (float)cn->getWidth(), (float)Helpers::SignalHeight);
				sb = p.getLocalArea(cn, sb);
				pinPositions.childPositions.push_back({ sb.getTopLeft(), sb.getTopRight(), idx });
			}
		}

		idx++;
	}
#endif

	

	if(!isSerialType() && pinPositions.childPositions.size() != connections.size())
	{
		init(parent.getValueTree());
	}

	parent.addButtons.clear();

	std::vector<std::pair<Path, int>> hitzonePaths;

	auto addPath = [&](Point<float> s, Point<float> e, int dropIndex, bool isVertical)
	{
		Path p;

		auto sh = (float)Helpers::SignalHeight;
		auto offset = getCableOffset(dropIndex, s, e);

		if(isVertical)		
		{
			auto s1 = s.translated(sh * -0.5f, 0.0f);
			auto s2 = s.translated(sh * 0.5f, 0.0f);

			auto e1 = e.translated(sh * -0.5f, 0.0f);
			auto e2 = e.translated(sh * 0.5f, 0.0f);

			p.startNewSubPath(s1);
			p.lineTo(s2);
			p.lineTo(e2);
			p.lineTo(e1);
			p.closeSubPath();

#if 0
			auto s1 = s.translated((float)Helpers::SignalHeight * -0.5f, 0.0f);
			auto s2 = s.translated((float)Helpers::SignalHeight *  0.5f, 0.0f);

			auto e1 = e.translated((float)Helpers::SignalHeight * -0.5f, 0.0f);
			auto e2 = e.translated((float)Helpers::SignalHeight * 0.5f, 0.0f);

			p.startNewSubPath(s1);

			Helpers::createCustomizableCurve(p, s1, e1, 0.0f, 2.0f, false);
			//Helpers::createCurve(p, s1, e1, true);
			p.lineTo(e2);
			Helpers::createCustomizableCurve(p, e2, s2, 0.0f, 2.0f, false);
			//Helpers::createCurve(p, e2, s2, true);
			p.closeSubPath();
#endif
		}
		else
		{


			auto mx = (e.getX() - s.getX()) * 0.5f;
			auto my = (e.getY() - s.getY());
			

			auto ms = sh * 0.5f;

			if(e.getY() < s.getY())
				ms *= -1.0f;

			p.startNewSubPath(s);

			auto pos = s.translated(mx + ms + offset, 0.0f);
			p.lineTo(pos);
			pos = pos.translated(0.0, my);
			p.lineTo(pos);
			pos = e;
			p.lineTo(pos);
			pos = pos.translated(0.0f, sh);
			p.lineTo(pos);
			pos = pos.translated(-mx - ms + offset, 0.0f);
			p.lineTo(pos);
			pos = pos.translated(0.0f, -my);
			p.lineTo(pos);
			pos = s.translated(0.0f, sh);
			p.lineTo(pos);
			p.closeSubPath();

			




			/*
			p.startNewSubPath(s);
			//Helpers::createCurve(p, s, e, isVertical);
			Helpers::createCustomizableCurve(p, s, e, 0.0f, 2.0f, false);

			auto me = e.translated(0.0f, (float)Helpers::SignalHeight);
			auto se = s.translated(0.0f, (float)Helpers::SignalHeight);

			p.lineTo(me);
			//Helpers::createCurve(p, me, se, isVertical);
			Helpers::createCustomizableCurve(p, me, se, 0.0f, 2.0f, false);
			p.closeSubPath();
			*/

			p = p.createPathWithRoundedCorners(4.0f);
			
			
		}
		
		hitzonePaths.push_back({p, dropIndex});
	};

	if(pinPositions.childPositions.empty())
	{
		addPath(pinPositions.containerStart.translated(10.0f, 0.0f), pinPositions.containerEnd.translated(-10.0f, 0.0f), 0, false);
	}
	else
	{
		addPath(pinPositions.containerStart.translated(10.0f, 0.0f), pinPositions.childPositions[0].first, 0, false);

		for(int i = 0; i < pinPositions.childPositions.size()-1; i++)
			addPath(pinPositions.childPositions[i].second, pinPositions.childPositions[i+1].first, pinPositions.childPositions[i].dropIndex + 1, isForcedVertical);

		addPath(pinPositions.childPositions.back().second, pinPositions.containerEnd.translated(-10.0f, 0.0f), pinPositions.childPositions.back().dropIndex+1, false);
	}

	for(const auto& hz: hitzonePaths)
	{
		parent.addButtons.add(new AddButton(parent, hz.first, hz.second));
	};

	p.repaint();
}

void ContainerComponent::onChildAddRemove(const ValueTree& v, bool wasAdded)
{
	if (wasAdded)
	{
		NodeComponent* nc;

		if (Helpers::isContainerNode(v))
		{
			if(v[PropertyIds::Locked])
				nc = new LockedContainerComponent(lasso, v, um);
			else
				nc = new ContainerComponent(lasso, v, um);
		}
		else if (Helpers::isProcessNode(v))
			nc = new ProcessNodeComponent(lasso, v, um);
		else
			nc = new NoProcessNodeComponent(lasso, v, um);

		addAndMakeVisible(nc);

		struct Sorter
		{
			static int compareElements(NodeComponent* n1, NodeComponent* n2)
			{
				auto v1 = n1->getValueTree();
				auto v2 = n2->getValueTree();

				auto idx1 = v1.getParent().indexOf(v1);
				auto idx2 = v2.getParent().indexOf(v2);

				if (idx1 < idx2)
					return -1;
				if (idx1 > idx2)
					return 1;

				return 0;
			}
		} sorter;

		childNodes.addSorted(sorter, nc);

		if(v[PropertyIds::Comment].toString().isNotEmpty())
		{
			comments.add(new Comment(*this, nc));
		}
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

		for(auto c: comments)
		{
			if(c->data == v)
			{
				comments.removeObject(c);
				break;
			}
		}
	}

	if (auto d = findParentComponentOfClass<CableComponent::CableHolder>())
		d->rebuildCables();

	cables.updatePins(*this);
}

void ContainerComponent::paint(Graphics& g)
{
	auto nodeColour = Helpers::getNodeColour(data);

	g.setColour(Colour(0x20000000));
	g.fillRect(getLocalBounds().removeFromLeft(Helpers::ParameterWidth));

	g.drawVerticalLine(Helpers::ParameterWidth-1, 0.0f, (float)getHeight());

	if (Helpers::isFoldedOrLockedContainer(getValueTree()) || !header.downPos.isOrigin())
	{
		g.fillAll(Colour(0xFF353535));
	}
	else
	{
		g.fillAll(Colour(0xFF222222).withAlpha(0.03f));
	}

	{
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

	g.setColour(nodeColour);
	g.drawRect(getLocalBounds(), 1);

	auto tb = getLocalBounds();
	tb.removeFromTop(Helpers::HeaderHeight);
	tb = tb.removeFromTop(Helpers::SignalHeight).removeFromLeft(Helpers::ParameterMargin + Helpers::ParameterWidth - 25).reduced(0, 2);

	g.setColour(nodeColour);
	description.draw(g, tb.toFloat());

	auto root = findParentComponentOfClass<DspNetworkComponent>();

	if (root->currentlyHoveredContainer != nullptr)
	{
		if(root->currentlyHoveredContainer == this)
		{
			if (cables.dropIndex == -1)
			{
				g.fillAll(Colour(SIGNAL_COLOUR).withAlpha(0.05f));
			}
		}
		else
		{
			g.fillAll(Colour(0xFF222222));
		}
	}

	drawOutsideLabel(g);

	g.setColour(Helpers::getNodeColour(data));
	cables.draw(g);

	for(auto cn: childNodes)
	{
		if(cn->getParentComponent() != this)
			continue;

		if(cn->selected)
		{
			auto tb = cn->getBoundsInParent().expanded(3.0f).toFloat();
			g.setColour(Colour(SIGNAL_COLOUR).withAlpha(0.8f));
			g.drawRoundedRectangle(tb, 3.0f, 1.0f);
		}

		auto b = cn->getBoundsInParent().toFloat().expanded(1.0f);
		g.setColour(Colours::black.withAlpha(0.2f));

		std::array<float, 5> alphas = { 0.25f, 0.15f, 0.1f, 0.05f, 0.02f };

		for(int i = 0; i < 5; i++)
		{
			g.setColour(Colours::black.withAlpha(alphas[i]));
			g.drawRect(b, 2.0f);
			b = b.translated(0.0f, 0.5f).expanded(0.25f);
		}
	}

	for(auto gr: groups)
		gr->draw(g);
}

void ContainerComponent::ParameterVerticalDragger::mouseDrag(const MouseEvent& e)
{
	auto newY = downY + e.getDistanceFromDragStartY();

	auto minY = Helpers::HeaderHeight + Helpers::SignalHeight;
	auto maxY = parent.getHeight() - parent.parameters.size() * Helpers::ParameterHeight;

	if (minY < maxY)
	{
		newY = jlimit(minY, maxY, newY);
		parent.getValueTree().setProperty(UIPropertyIds::ParameterYOffset, newY, parent.um);
	}
}

void ContainerComponent::ParameterVerticalDragger::onOffset(const Identifier& id, const var& newValue)
{
	auto offset = jmax(Helpers::HeaderHeight + Helpers::SignalHeight, (int)newValue);
	setTopLeftPosition(0, offset);
	parent.resized();

	if(auto ch = findParentComponentOfClass<CableComponent::CableHolder>())
		ch->rebuildCables();
}

void ContainerComponent::Resizer::mouseUp(const MouseEvent& e)
{
	auto parent = findParentComponentOfClass<ContainerComponent>();

	Helpers::updateBounds(parent->getValueTree(), parent->getBoundsInParent(), parent->um);

	auto root = findParentComponentOfClass<DspNetworkComponent>();
	Helpers::fixOverlap(Helpers::getCurrentRoot(parent->getValueTree()), &root->um, false);
}

struct PopupCodeEditor: public Component,
						public CodeDocument::Listener,
						public Timer
{
	PopupCodeEditor(const ValueTree& v, const Identifier& id_, UndoManager* um_):
	  d(v),
	  id(id_),
	  um(um_),
	  doc(codeDoc),
	  editor(doc)
	{
		if(id == PropertyIds::Comment)
			editor.setLanguageManager(new mcl::MarkdownLanguageManager());
		
		codeDoc.replaceAllContent(v[id].toString());
		codeDoc.clearUndoHistory();

		codeDoc.addListener(this);

		addAndMakeVisible(editor);
		setSize(400, 300);
	}

	~PopupCodeEditor()
	{
		updateValue();
		stopTimer();
		codeDoc.removeListener(this);
	}

	void updateValue()
	{
		d.setProperty(id, codeDoc.getAllContent(), um);
	}

	static void show(Component* attachedComponent, const ValueTree& data, const Identifier& id, UndoManager* um)
	{
		auto root = attachedComponent->findParentComponentOfClass<DspNetworkComponent>();
		
		std::unique_ptr<Component> te;

		auto editor = new PopupCodeEditor(data, id, um);
		
		auto rootPos = root->getLocalArea(attachedComponent, attachedComponent->getLocalBounds());

		editor->setSize(400, 300);
		te.reset(editor);

		CallOutBox::launchAsynchronously(std::move(te), attachedComponent->getScreenBounds(), nullptr);
	}

	void resized() override
	{
		editor.setBounds(getLocalBounds());
	}

	void timerCallback() override
	{
		updateValue();
		stopTimer();
	}

	void codeDocumentTextDeleted(int startIndex, int endIndex) override
	{
		startTimer(500);
	}

	void codeDocumentTextInserted(const String& newText, int insertIndex) override
	{
		startTimer(500);
	}

	ValueTree d;
	const Identifier id;
	UndoManager* um;

	juce::CodeDocument codeDoc;
	mcl::TextDocument doc;
	mcl::TextEditor editor;
};

void ContainerComponent::Comment::showEditor()
{
	PopupCodeEditor::show(this, data, PropertyIds::Comment, parent.um);
}

void ContainerComponent::AddButton::mouseUp(const MouseEvent& e)
{
	if(SelectableComponent::Lasso::checkLassoEvent(e, SelectableComponent::LassoState::Up))
		return;

	if(e.mouseWasDraggedSinceMouseDown())
	{
		parent.cables.updatePins(parent);
		return;
	}

	if (active)
	{
		BuildHelpers::CreateData cd;
		cd.pointInContainer = e.getEventRelativeTo(&parent).getPosition();
		cd.containerToInsert = parent.getValueTree();
		cd.source = nullptr;
		cd.signalIndex = index;

		auto root = findParentComponentOfClass<DspNetworkComponent>();
		
		auto globalPos = root->getLocalPoint(&parent, cd.pointInContainer);

		auto copy = hitzone;

		auto delta = root->getLocalPoint(this, hitzone.getBounds().getTopLeft().toInt());

		copy.applyTransform(AffineTransform::translation(delta));
		
		root->showCreateNodePopup(globalPos, copy, cd);
	}
}

void ContainerComponent::AddButton::mouseMove(const MouseEvent& e)
{
	auto isActive = true;

	if (isActive != active)
	{
		active = isActive;
		repaint();
	}
}

void ContainerComponent::Group::draw(Graphics& g)
{
	RectangleList<int> bounds;

	auto nodeTree = parent.getValueTree().getChildWithName(PropertyIds::Nodes);

	Colour c;

	for (auto n : nodeTree)
	{
		if (idList.contains(n[PropertyIds::ID].toString()))
		{
			c = Helpers::getNodeColour(n);
			bounds.addWithoutMerging(Helpers::getBounds(n, true));
		}
	}

	auto b = bounds.getBounds().expanded(10).toFloat();

	b = b.withTop(b.getY() - 20.0f);

	g.setColour(c.withAlpha(0.05f));
	g.fillRoundedRectangle(b, 5.0f);
	g.setColour(c.withAlpha(0.6f));
	g.drawRoundedRectangle(b, 5.0f, 1.0f);

	lastBounds = b;

	auto f = GLOBAL_BOLD_FONT();

	name = v[PropertyIds::ID].toString();

	g.setColour(c);
	g.setFont(f);
	g.drawText(name, b.removeFromTop(20.0f), Justification::centred);

	
}

}