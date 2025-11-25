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


struct SelectableComponent: public ChangeListener
{
	using WeakPtr = WeakReference<SelectableComponent>;
	using Selection = SelectedItemSet<WeakPtr>;

	enum class LassoState
	{
		Down,
		Drag,
		Up
	};

	struct Lasso : public juce::LassoSource<WeakPtr>,
				   public Timer,
				   public ChangeListener
	{
		Lasso()
		{
			startTimer(500);
			selection.addChangeListener(this);
		}

		virtual ~Lasso() 
		{
			selection.removeChangeListener(this);
		};

		void changeListenerCallback(ChangeBroadcaster* cb) override
		{
			cleanupSelection();
		}

		void forEach(const Array<ValueTree>& nodes, const std::function<void(SelectableComponent*)>& f)
		{
			Component::callRecursive<SelectableComponent>(dynamic_cast<Component*>(this), [&](SelectableComponent* nc)
			{
				if (nodes.contains(nc->getValueTree()))
					f(nc);

				return false;
			});
		}

		void timerCallback() override
		{
			um.beginNewTransaction();
		}

		Array<ValueTree> createTreeListFromSelection(const Identifier& typeMatch=PropertyIds::Node) const
		{
			Array<ValueTree> list;

			for(auto& s: selection.getItemArray())
			{
				if(s.get() != nullptr)
				{
					auto v = s->getValueTree();

					if(typeMatch.isNull() || typeMatch == v.getType())
						list.add(v);
				}
			}

			return list;
		}

		static bool checkLassoEvent(const MouseEvent& e, LassoState state)
		{
			auto l = dynamic_cast<Lasso*>(e.eventComponent);

			if(e.mods.isShiftDown() || l != nullptr)
			{
				if(l == nullptr)
					l = e.eventComponent->findParentComponentOfClass<Lasso>();

				auto le = e.getEventRelativeTo(dynamic_cast<Component*>(l));

				if(state == LassoState::Down)
					l->lasso.beginLasso(le, l);
				if(state == LassoState::Drag)
					l->lasso.dragLasso(le);
				if(state == LassoState::Up)
					l->lasso.endLasso();
				
				return true;
			}

			return false;
		}

		void setSelection(const Array<ValueTree>& nodes)
		{
			std::vector<SelectableComponent*> matches;
			matches.reserve(nodes.size());

			forEach(nodes, [&](SelectableComponent* nc) { matches.push_back(nc); });

			getLassoSelection().deselectAll();

			for (auto& m : matches)
				getLassoSelection().addToSelection(m);
		}

		

		Array<ValueTree> clipboard;

		void colourSelection()
		{
			auto c = Colours::red.withHue(Random::getSystemRandom().nextFloat());
			c = c.withBrightness(0.7f);
			c = c.withSaturation(0.5f);

			for(auto s: selection.getItemArray())
			{
				auto vt = s->getValueTree();

				if(vt.getType() == PropertyIds::Node)
				{
					vt.setProperty(PropertyIds::NodeColour, c.getARGB(), &um);
				}
			}
		}

		Result copySelection()
		{
			clipboard.clear();

			const auto& list = getLassoSelection().getItemArray();

			for (auto& s : list)
			{
				if(s->isNode())
					clipboard.add(s->getValueTree());
			}

			return Result::ok();
		}

		Result deleteSelection()
		{
			std::vector<ValueTree> toBeRemoved;

			for (auto nc : getLassoSelection().getItemArray())
				toBeRemoved.push_back(nc->getValueTree());
			
			for(const auto& v: toBeRemoved)
				v.getParent().removeChild(v, &um);

			return Result::ok();
		}

		Result cutSelection()
		{
			auto ok = copySelection();

			deleteSelection();

			return ok;
		}

		Result create(const Array<ValueTree>& list, Point<int> startPoint)
		{
			if (list.isEmpty())
				return Result::fail("No selection");

			auto container = list.getFirst().getParent().getParent();
			jassert(container.getType() == PropertyIds::Node);
			auto isVertical = Helpers::shouldBeVertical(container);

			int insertIndex = -1;
			RectangleList<int> positions;

			for (const auto& d : list)
			{
				insertIndex = d.getParent().indexOf(d) + 1;
				positions.add(Helpers::getBounds(d, true));
			}

			// make it overlap for the fix algorithm to position them properly
			int deltaX = isVertical ? 0 : (positions.getBounds().getWidth() - 10);
			int deltaY = isVertical ? (positions.getBounds().getHeight() - 10) : 0;

			if (!startPoint.isOrigin())
			{
				deltaX = startPoint.getX() - positions.getBounds().getX();
				deltaY = startPoint.getY() - positions.getBounds().getY();
			}

			auto rootTree = valuetree::Helpers::findParentWithType(list[0], PropertyIds::Network);

			Array<ValueTree> newTrees;

			for (auto& n : list)
			{
				auto copy = n.createCopy();

				auto n = Helpers::getHeaderTitle(copy);

				copy.setProperty(PropertyIds::Name, n, nullptr);
				auto newId = Helpers::getUniqueId(copy[PropertyIds::ID].toString(), rootTree);
				copy.setProperty(PropertyIds::ID, newId, nullptr);

				Helpers::translatePosition(copy, { deltaX, deltaY }, nullptr);
				container.getChildWithName(PropertyIds::Nodes).addChild(copy, insertIndex++, &um);
				newTrees.add(copy);
			}

			Helpers::fixOverlap(rootTree.getChildWithName(PropertyIds::Node), &um, false);

			MessageManager::callAsync([newTrees, this]()
			{
				setSelection(newTrees);

				auto asC = dynamic_cast<Component*>(this);

				auto zp = asC->findParentComponentOfClass<ZoomableViewport>();

				RectangleList<int> all;

				for(auto& s: selection.getItemArray())
				{
					auto sc = dynamic_cast<Component*>(s.get());

					if(sc != nullptr)
						all.addWithoutMerging(asC->getLocalArea(sc, sc->getLocalBounds()));
				}

				zp->scrollToRectangle(all.getBounds(), true, true);
			});

			return Result::ok();
		}

		Result pasteSelection(Point<int> startPoint)
		{
			return create(clipboard, startPoint);
		}

		void groupSelection();

		Result duplicateSelection()
		{
			Array<ValueTree> trees;

			for (auto n : getLassoSelection().getItemArray())
			{
				if(n != nullptr && n->isNode())
					trees.add(n->getValueTree());
			}

			return create(trees, {});
		}

		Selection& getLassoSelection() override
		{
			return selection;
		}

		void cleanupSelection()
		{
			auto currentSelection = selection.getItemArray();

			for(auto& s: currentSelection)
			{
				if(s.get() == nullptr)
					selection.deselect(s);
			}

			for(auto& s: currentSelection)
			{
				auto thisType = s->getValueTree().getType();

				for(auto other: currentSelection)
				{
					if((other->getValueTree().getType() == thisType) &&
						s->getValueTree().isAChildOf(other->getValueTree()))
					{
						selection.deselect(other);
						break;
					}
				}
			}
		}

		UndoManager um;
		juce::LassoComponent<SelectableComponent::WeakPtr> lasso;
		Selection selection;

		scriptnode::NodeDatabase db;
	};

	SelectableComponent(Lasso& l):
	  lasso(l)
	{
		lasso.getLassoSelection().addChangeListener(this);
	}

	~SelectableComponent() override
	{
		lasso.getLassoSelection().removeChangeListener(this);
	};

	bool isNode() const { return getValueTree().getType() == PropertyIds::Node; }

	void changeListenerCallback(ChangeBroadcaster* cb) override
	{
		auto lasso = dynamic_cast<Selection*>(cb);

		auto isSelected = lasso->getItemArray().contains(this);

		if (isSelected != selected)
		{
			selected = isSelected;

			auto c = dynamic_cast<Component*>(this);

			if(auto p = c->getParentComponent())
				p->repaint(c->getBoundsInParent().expanded(10));
		}
	}

	virtual ValueTree getValueTree() const = 0;

	Lasso& lasso;
	bool selected = false;

	JUCE_DECLARE_WEAK_REFERENCEABLE(SelectableComponent);
};

struct NodeComponent : public Component,
	public SelectableComponent,
	public PathFactory
{
	struct HeaderComponent : public CablePinBase,
							 public ComponentBoundsConstrainer
	{
		HeaderComponent(NodeComponent& parent_, const ValueTree& v, UndoManager* um_) :
			CablePinBase(v, um_),
			parent(parent_),
			powerButton("bypass", nullptr, parent_),
			closeButton("delete", nullptr, parent_),
			dragger()
		{
			addAndMakeVisible(powerButton);
			addAndMakeVisible(closeButton);

			powerButton.setToggleModeWithColourChange(true);

			powerButton.onClick = [this]()
			{
				parent.toggle(PropertyIds::Bypassed);
				parent.repaint();
			};

			powerButton.setToggleStateAndUpdateIcon(!parent.getValueTree()[PropertyIds::Bypassed]);

			closeButton.onClick = [this]()
			{
				auto data = parent.getValueTree();
				data.getParent().removeChild(data, parent.um);
			};
		};

		void resized() override
		{
			powerButton.setVisible(parent.hasSignal);

			auto b = getLocalBounds();
			powerButton.setBounds(b.removeFromLeft(getHeight()).reduced(2));
			closeButton.setBounds(b.removeFromRight(getHeight()).reduced(2));
		}

		Helpers::ConnectionType getConnectionType() const override { return Helpers::ConnectionType::Bypassed; };

		void checkBounds (Rectangle<int>& bounds, const Rectangle<int>&, const Rectangle<int>&, bool, bool, bool,bool ) override
		{
			auto minX = Helpers::ParameterWidth + Helpers::NodeMargin;
			auto minY = Helpers::HeaderHeight + 10;

			auto offsetX = 0;
			auto offsetY = 0;

			Rectangle<int> possible(minX + offsetY, minY + offsetY, 10000000, 10000000);

			bounds = bounds.constrainedWithin(possible);
		}

		bool matchesConnectionType(Helpers::ConnectionType ct) const override
		{
			return ct == Helpers::ConnectionType::Parameter;
		};

		bool canBeTarget() const override { return true; }

		String getTargetParameterId() const override { return "Bypassed"; }

		ValueTree getConnectionTree() const override { return {}; }

		void mouseDoubleClick(const MouseEvent& event) override
		{
			parent.toggle(PropertyIds::Folded);
			Helpers::fixOverlap(parent.getValueTree(), parent.um, false);
		}

		void mouseDown(const MouseEvent& e) override;

		Point<int> downPos;
		bool swapDrag = false;
		Component* originalParent = nullptr;
		Rectangle<int> originalBounds;

		std::map<SelectableComponent*, Rectangle<int>> selectionPositions;

		void mouseDrag(const MouseEvent& e) override;

		void mouseUp(const MouseEvent& e) override;

		void paint(Graphics& g) override
		{
			auto b = getLocalBounds().toFloat();
			g.setColour(Helpers::getNodeColour(parent.getValueTree()));
			g.fillRect(b);
			g.setColour(Colours::white.withAlpha(0.1f));
			g.fillRect(getLocalBounds().removeFromTop(1));
			g.setColour(Colours::white);

			

			if (powerButton.isVisible())
				b.removeFromLeft(getHeight());
			else
				b.removeFromLeft(5);

			g.setFont(GLOBAL_FONT());
			g.drawText(Helpers::getHeaderTitle(parent.getValueTree()), b, Justification::left);
		}

		NodeComponent& parent;
		HiseShapeButton closeButton;
		HiseShapeButton powerButton;
		juce::ComponentDragger dragger;
	};

	void toggle(const Identifier& id)
	{
		auto v = (bool)data[id];
		data.setProperty(id, !v, um);
	}

	NodeComponent(Lasso& l, const ValueTree& v, UndoManager* um_, bool useSignal) :
		SelectableComponent(l),
		data(v),
		um(um_),
		header(*this, v, um),
		hasSignal(useSignal)
	{
		

		addAndMakeVisible(header);

		positionListener.setCallback(v,
			{ UIPropertyIds::x, UIPropertyIds::y },
			Helpers::UIMode,
			VT_BIND_PROPERTY_LISTENER(onPositionUpdate));

		foldListener.setCallback(v,
			{ PropertyIds::Folded },
			Helpers::UIMode,
			VT_BIND_PROPERTY_LISTENER(onFold));

		setBounds(Helpers::getBounds(v, false));

		for (auto p : data.getChildWithName(PropertyIds::Parameters))
		{
			addAndMakeVisible(parameters.add(new ParameterComponent(p, um)));
		}

		auto modOutput = data.getChildWithName(PropertyIds::ModulationTargets);

		if (modOutput.isValid())
		{
			addAndMakeVisible(modOutputs.add(new ModOutputComponent(modOutput, um)));
		}

		for (auto st : data.getChildWithName(PropertyIds::SwitchTargets))
			addAndMakeVisible(modOutputs.add(new ModOutputComponent(st, um)));

		colourListener.setCallback(data, { PropertyIds::NodeColour }, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onColour));

		resized();
	};

	virtual ~NodeComponent() = default;
	
	void onColour(const Identifier&, const var&)
	{
		repaint();
		header.repaint();
	}

	Path createPath(const String& url) const override
	{
		Path p;

		LOAD_EPATH_IF_URL("bypass", HiBinaryData::ProcessorEditorHeaderIcons::bypassShape);
		LOAD_EPATH_IF_URL("delete", HiBinaryData::ProcessorEditorHeaderIcons::closeIcon);
		LOAD_EPATH_IF_URL("lock", EditorIcons::lockShape);
		LOAD_EPATH_IF_URL("add", HiBinaryData::ProcessorEditorHeaderIcons::addIcon);

		return p;
	}

	void onPositionUpdate(const Identifier&, const var&)
	{
		setTopLeftPosition(Helpers::getPosition(data));
	}

	virtual void onFold(const Identifier& id, const var& newValue)
	{
		auto boundsToUse = Helpers::getBounds(getValueTree(), false);
		setBounds(boundsToUse);
	}

	void resized() override
	{
		auto b = getLocalBounds();
		header.setBounds(b.removeFromTop(Helpers::HeaderHeight));

		if (hasSignal)
			b.removeFromTop(Helpers::SignalHeight);

		Rectangle<int> pb;

		if(!parameters.isEmpty())
			pb = b.removeFromLeft(Helpers::ParameterMargin + Helpers::ParameterWidth);

		pb.removeFromLeft(Helpers::ParameterMargin);

		auto folded = data[PropertyIds::Folded];

		for(auto p: parameters)
			p->setVisible(!folded);

		for(auto m: modOutputs)
			m->setVisible(!folded);

		if(folded)
			return;

		auto mb = b.removeFromRight(Helpers::ParameterWidth);

		for (auto p: parameters)
		{
			pb.removeFromTop(Helpers::ParameterMargin);
			p->setBounds(pb.removeFromTop(getParameterHeight()));
		}

		for (auto m : modOutputs)
		{
			m->setBounds(mb.removeFromTop(24));
		}
	}

	ValueTree getValueTree() const override { return data; }

	const bool hasSignal;

	int getParameterHeight() const { return Helpers::ParameterHeight; }

	void setFixSize(Rectangle<int> extraBounds)
	{
		int x = 0;

		if (!parameters.isEmpty())
			x += Helpers::ParameterMargin + Helpers::ParameterWidth;

		int y = Helpers::HeaderHeight;

		if(!hasSignal)
			x += extraBounds.getWidth();

		if (!modOutputs.isEmpty())
			x += Helpers::ParameterWidth;

		if (Helpers::hasRoutableSignal(getValueTree()))
			y += Helpers::SignalHeight;

		if (hasSignal)
			y += Helpers::SignalHeight;

		int bodyHeight = 0;

		auto parameterHeight = jmax(parameters.size() * (Helpers::ParameterMargin + getParameterHeight()), modOutputs.size() * 24) + Helpers::ParameterMargin;

		if(hasSignal)
			bodyHeight = extraBounds.getHeight() + parameterHeight;
		else
			bodyHeight = jmax(extraBounds.getHeight(), parameterHeight);

		x = jmax(x, GLOBAL_FONT().getStringWidth(Helpers::getHeaderTitle(getValueTree())) + 20 + 2 * Helpers::HeaderHeight);

		if(hasSignal)
			x = jmax(x, extraBounds.getWidth());

		Rectangle<int> nb(getPosition(), getPosition().translated(x, y + bodyHeight));

		Helpers::updateBounds(getValueTree(), nb, um);

		setSize(x, y + bodyHeight);
		resized();
	}

	void mouseDown(const MouseEvent& e) override
	{
		CHECK_MIDDLE_MOUSE_DOWN(e);
	}

	void mouseDrag(const MouseEvent& e) override
	{
		CHECK_MIDDLE_MOUSE_DRAG(e);
	}

	void mouseUp(const MouseEvent& e) override
	{
		CHECK_MIDDLE_MOUSE_UP(e);
	}

	ParameterComponent* getParameter(int index) const { return parameters[index]; }
	int getNumParameters() const { return parameters.size(); }

	WeakReference<CablePinBase> getFirstModOutput() { return modOutputs.getFirst(); }

protected:

	

	ValueTree data;
	UndoManager* um;

	OwnedArray<ParameterComponent> parameters;
	OwnedArray<ModOutputComponent> modOutputs;

	valuetree::PropertyListener positionListener;
	valuetree::PropertyListener foldListener;
	valuetree::PropertyListener colourListener;

	HeaderComponent header;
};

}