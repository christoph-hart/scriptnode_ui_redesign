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

	if(p.getValueTree()[PropertyIds::Folded])
	{
		p.repaint();
		return;
	}

	auto root = p.findParentComponentOfClass<DspNetworkComponent>();

	if(root == nullptr)
		return;

	int idx = 0;

	for (auto cn : p.childNodes)
	{
		if (root->currentlyDraggedComponents.contains(cn))
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

	

	if(!isSerialType() && pinPositions.childPositions.size() != connections.size())
	{
		init(parent.getValueTree());
	}

	parent.addButtons.clear();

	auto hitzones = pinPositions.makeHitZones(parent.getLocalBounds(), isVertical());

	for(auto hz: hitzones)
	{
		parent.addButtons.add(new AddButton(parent, hz.second, hz.first));
	}

	p.repaint();
}

void ContainerComponent::onChildAddRemove(const ValueTree& v, bool wasAdded)
{
	if (wasAdded)
	{
		NodeComponent* nc;

		if (Helpers::isContainerNode(v))
			nc = new ContainerComponent(lasso, v, um);
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
	}

	if (auto d = findParentComponentOfClass<CableComponent::CableHolder>())
		d->rebuildCables();

	cables.updatePins(*this);
}

void ContainerComponent::paint(Graphics& g)
{
	auto nodeColour = Helpers::getNodeColour(data);

	if (data[PropertyIds::Folded] || !header.downPos.isOrigin())
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

	
	

	g.setColour(Colours::black.withAlpha(0.2f));
	g.fillRoundedRectangle(tb.translated(5.0f, 0.0f).toFloat(), 4.0f);
	g.setColour(nodeColour);
	description.draw(g, tb.toFloat());

	auto root = findParentComponentOfClass<DspNetworkComponent>();

	if (cables.dropIndex != -1)
	{
		g.fillAll(Colour(SIGNAL_COLOUR).withAlpha(0.05f));
	}

	if(!outsideLabel.isEmpty())
	{
		g.setFont(GLOBAL_FONT());
		g.drawText("External Parameters", outsideLabel.toFloat(), Justification::centred);
	}

	g.setColour(Helpers::getNodeColour(data));
	cables.draw(g);
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
	Helpers::fixOverlap(root->rootComponent->getValueTree(), &root->um, false);
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
		codeDoc.removeListener(this);
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
		d.setProperty(id, codeDoc.getAllContent(), um);
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

	if (DspNetworkComponent::isEditModeEnabled(e))
	{
		ValueTree n(PropertyIds::Node);
		n.setProperty(PropertyIds::FactoryPath, "math.mul", nullptr);
		n.setProperty(PropertyIds::ID, "math" + String(index), nullptr);
		n.setProperty(PropertyIds::NodeColour, Colours::red.withHue(Random::getSystemRandom().nextFloat()).withSaturation(0.5f).withBrightness(0.4f).getARGB(), nullptr);

		auto pos = e.getEventRelativeTo(&parent).getPosition();

		auto nodes = parent.getValueTree().getChildWithName(PropertyIds::Nodes);

		auto prevPos = Helpers::getPosition(nodes.getChild(index - 1));
		auto nextPos = Helpers::getPosition(nodes.getChild(index));

		auto y = prevPos.getY() + (nextPos.getY() - prevPos.getY()) / 2;


		n.setProperty(UIPropertyIds::x, pos.getX(), nullptr);
		n.setProperty(UIPropertyIds::y, y, nullptr);

		ValueTree ps(PropertyIds::Parameters);
		ValueTree p(PropertyIds::Parameter);
		p.setProperty(PropertyIds::ID, "Value", nullptr);
		ps.addChild(p, -1, nullptr);
		n.addChild(ps, -1, nullptr);

		nodes.addChild(n, index, parent.um);

		Helpers::fixOverlap(parent.getValueTree(), parent.um, false);
	}
}

void ContainerComponent::AddButton::mouseMove(const MouseEvent& e)
{
	auto isActive = DspNetworkComponent::isEditModeEnabled(e);

	if (isActive != active)
	{
		active = isActive;
		setMouseCursor(isActive ? MouseCursor::CopyingCursor : MouseCursor::NormalCursor);
		repaint();
	}
}

}