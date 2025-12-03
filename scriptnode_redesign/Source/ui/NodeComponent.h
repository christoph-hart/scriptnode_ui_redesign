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
						list.addIfNotAlreadyThere(v);
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
			cleanupSelection();

			std::vector<ValueTree> toBeRemoved;

			for (auto nc : getLassoSelection().getItemArray())
				toBeRemoved.push_back(nc->getValueTree());
			
			for(const auto& v: toBeRemoved)
			{
				BuildHelpers::cleanBeforeDelete(v, &um);
				v.getParent().removeChild(v, &um);
			}
			
			selection.deselectAll();

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
				auto t = Helpers::getHeaderTitle(copy);
				copy.setProperty(PropertyIds::Name, t, nullptr);
				Helpers::translatePosition(copy, { deltaX, deltaY }, nullptr);
				newTrees.add(copy);
			}


			BuildHelpers::updateIds(rootTree, newTrees);

			for(auto n: newTrees)
				container.getChildWithName(PropertyIds::Nodes).addChild(n, insertIndex++, &um);

			Helpers::updateChannelCount(rootTree, false, &um);
			Helpers::fixOverlap(Helpers::getCurrentRoot(container), &um, false);

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

		bool navigateSelection(const KeyPress& k)
		{
			if(selection.getNumSelected() == 0)
				return false;

			auto inc = k == KeyPress::rightKey || k == KeyPress::downKey;
			auto sibling = k == KeyPress::leftKey || k == KeyPress::rightKey;

			auto currentNodes = createTreeListFromSelection(PropertyIds::Node);
				
			Array<ValueTree> nextNodes;

			RectangleList<int> bounds;

			for(const auto& s: selection.getItemArray())
			{
				if(s.get() != nullptr)
				{
					auto vt = s->getValueTree();

					if(vt.getType() != PropertyIds::Node)
						continue;

					if(sibling)
					{
						auto idx = vt.getParent().indexOf(vt);

						auto newIndex = jlimit<int>(0, vt.getParent().getNumChildren()-1, idx + (inc ? 1 : -1));
						nextNodes.add(vt.getParent().getChild(newIndex));
					}
					else
					{
						ValueTree nextNode;

						if(inc)
							nextNode = vt.getChildWithName(PropertyIds::Nodes).getChild(0);
						else
							nextNode = valuetree::Helpers::findParentWithType(vt, PropertyIds::Node);
							
						if (nextNode.isValid())
							nextNodes.add(nextNode);
					}
				}
			}

			for(const auto& n: nextNodes)
			{
				bounds.addWithoutMerging(Helpers::getBoundsInRoot(n, false));
			}

			if(k.getModifiers().isCommandDown() || k.getModifiers().isShiftDown())
			{
				for(auto s: currentNodes)
					nextNodes.addIfNotAlreadyThere(s);
			}

			setSelection(nextNodes);

			auto zp = dynamic_cast<Component*>(this)->findParentComponentOfClass<ZoomableViewport>();

			if(zp != nullptr)
				zp->zoomToRectangle(bounds.getBounds());

			return !nextNodes.isEmpty();
		}

		void cleanupSelection()
		{
			auto currentSelection = selection.getItemArray();

			for(auto& s: currentSelection)
			{
				if(s.get() == nullptr)
					selection.deselect(s);
			}

			currentSelection = selection.getItemArray();

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

		JUCE_DECLARE_WEAK_REFERENCEABLE(Lasso);
	};

	SelectableComponent(Lasso* l):
	  lasso(l)
	{
		if(lasso != nullptr)
			lasso->getLassoSelection().addChangeListener(this);
	}

	~SelectableComponent() override
	{
		if(lasso != nullptr)
			lasso->getLassoSelection().removeChangeListener(this);
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

	Lasso* lasso;
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

			if(!Helpers::isRootNode(v))
				addChildComponent(closeButton);

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
				BuildHelpers::cleanBeforeDelete(data, parent.um);
				data.getParent().removeChild(data, parent.um);
			};

			if(Helpers::isRootNode(v))
			{
                auto v = parent.getValueTree();
				valuetree::Helpers::forEachParent(v, [this](ValueTree& n)
				{
					if(n.getType() == PropertyIds::Node && n != parent.getValueTree())
					{
						addAndMakeVisible(breadcrumbs.add(new BreadcrumbButton(n)));
					}
						
					return false;
				});
			}
		};

		

		void mouseEnter(const MouseEvent& e) override
		{
			closeButton.setVisible(true);
		}

		void mouseExit(const MouseEvent& e) override
		{
			closeButton.setVisible(getLocalBounds().contains(e.getPosition()));
		}

		void resized() override
		{
			powerButton.setVisible(parent.hasSignal);

			auto b = getLocalBounds();
			powerButton.setBounds(b.removeFromLeft(getHeight()).reduced(2));

			for(int i = breadcrumbs.size() - 1; i >= 0; i--)
				breadcrumbs[i]->setBounds(b.removeFromLeft(breadcrumbs[i]->getWidth()));

			closeButton.setBounds(b.removeFromRight(getHeight()).reduced(2));
		}

		String getSourceDescription() const override { return ""; }

		Helpers::ConnectionType getConnectionType() const override { return Helpers::ConnectionType::Bypassed; };

		bool shiftDown = false;

		void checkBounds (Rectangle<int>& bounds, const Rectangle<int>&, const Rectangle<int>&, bool, bool, bool,bool ) override
		{
			auto minX = Helpers::ParameterWidth + Helpers::NodeMargin;
			auto minY = Helpers::HeaderHeight + 10;

			auto offsetX = 0;
			auto offsetY = 0;

			auto x = bounds.getX();
			auto y = bounds.getY();

			if(shiftDown)
			{
				x -= x % 100;
				y -= y % 100;
			}

			auto childNodes = parent.getValueTree().getParent();

			for(auto cn: childNodes)
			{
				if(cn == parent.getValueTree())
					continue;

				bool found = false;

				for(const auto& s: selectionPositions)
				{
					if(s.first->getValueTree() == cn)
					{
						found = true;
						break;
					}
						
				}

				if(found)
					continue;

				auto cnPos = Helpers::getPosition(cn);

				auto cx = cnPos.getX();
				auto cy = cnPos.getY();

				auto dx = std::abs(cx - x);
				auto dy = std::abs(cy - y);

				if(dx < 20)
					x = cx;
				else if(dy < 20)
					y = cy;
			}

			bounds.setPosition(x, y);

			Rectangle<int> possible(minX + offsetY, minY + offsetY, 10000000, 10000000);

			bounds = bounds.constrainedWithin(possible);
		}

		bool matchesConnectionType(Helpers::ConnectionType ct) const override
		{
			return ct != Helpers::ConnectionType::RoutableSignal;
		};

		Result getTargetErrorMessage(const ValueTree& requestedSource) const override
		{
			if(!ParameterHelpers::isSoftBypassNode(data))
				return Result::fail("Only connectable to a container.soft_bypass");

			auto pn = Helpers::findParentNode(requestedSource);

			if(!data.isAChildOf(pn))
				return Result::fail("Can't connect to outside bypass");

			return CablePinBase::getTargetErrorMessage(requestedSource);
		}

		bool canBeTarget() const override { return true; }

		String getTargetParameterId() const override { return "Bypassed"; }

		ValueTree getConnectionTree() const override { return {}; }

		void mouseDoubleClick(const MouseEvent& event) override;

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

			for(auto bc: breadcrumbs)
				b.removeFromLeft(bc->getWidth());

			g.drawText(Helpers::getHeaderTitle(parent.getValueTree()), b, Justification::left);
		}

		struct BreadcrumbButton: public Component
		{
			BreadcrumbButton(const ValueTree& n):
			  data(n)
			{
				setMouseCursor(MouseCursor::PointingHandCursor);
				jassert(n.getType() == PropertyIds::Node);

				auto w = GLOBAL_FONT().getStringWidth(Helpers::getHeaderTitle(n)) + 20 + 6;
				auto h = Helpers::HeaderHeight;
				setSize(w, h);
				setRepaintsOnMouseActivity(true);
			}

			void paint(Graphics& g) override
			{
				g.setFont(GLOBAL_FONT());

				float alpha = 0.5f;
				if(isMouseOver())
					alpha += 0.2f;

				if(isMouseButtonDown())
					alpha += 0.1;

				auto b = getLocalBounds().toFloat().reduced(3.0f, 0.0f);

				auto rb = b.removeFromRight(20.0f);

				g.setColour(Colours::white.withAlpha(0.1f));
				g.drawText("::", rb, Justification::centred);
				g.setColour(Colours::white.withAlpha(alpha));
				g.drawText(Helpers::getHeaderTitle(data), b, Justification::left);
			}

			void mouseUp(const MouseEvent& e) override;

			ValueTree data;
		};

		NodeComponent& parent;
		HiseShapeButton closeButton;
		HiseShapeButton powerButton;

		OwnedArray<BreadcrumbButton> breadcrumbs;

		juce::ComponentDragger dragger;
	};

	struct PopupComponent: public Component
	{
		PopupComponent(const ValueTree& v, UndoManager* um_):
		  data(v),
		  um(um_)
		{
			dummyBody = DummyBody::createDummyComponent(v[PropertyIds::FactoryPath].toString());

			if(dummyBody != nullptr)
				addAndMakeVisible(dummyBody);

			for (auto p : data.getChildWithName(PropertyIds::Parameters))
			{
				addAndMakeVisible(parameters.add(new ParameterComponent(p, um)));
			}

			auto modOutput = data.getChildWithName(PropertyIds::ModulationTargets);

			if (modOutput.isValid())
				addAndMakeVisible(modOutputs.add(new ModOutputComponent(modOutput, um)));

			for (auto st : data.getChildWithName(PropertyIds::SwitchTargets))
				addAndMakeVisible(modOutputs.add(new ModOutputComponent(st, um)));

			auto w = 0;

			if(!parameters.isEmpty())
				w += Helpers::ParameterMargin + Helpers::ParameterWidth;

			if(!modOutputs.isEmpty())
				w += Helpers::ModulationOutputWidth;

			auto h = parameters.size() * (Helpers::ParameterHeight + Helpers::ParameterMargin);

			h = jmax(h, modOutputs.size() * (Helpers::ModulationOutputHeight + Helpers::ParameterMargin));

			if(dummyBody != nullptr)
			{
				w = jmax(dummyBody->getWidth(), w);
				h += dummyBody->getHeight();
			}

			setSize(w, h);
		}

		void paint(Graphics& g) override
		{
			g.setColour(Colour(0xFF353535));
			g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
		}

		void resized() override
		{
			auto b = getLocalBounds();

			if(dummyBody != nullptr)
			{
				auto db = b.removeFromTop(dummyBody->getHeight());
				db = db.withSizeKeepingCentre(dummyBody->getWidth(), dummyBody->getHeight());
				dummyBody->setBounds(db);
			}

			if(!parameters.isEmpty())
			{
				b.removeFromLeft(Helpers::ParameterMargin);
				auto pb = b.removeFromLeft(Helpers::ParameterWidth);

				for(auto p: parameters)
				{
					p->setBounds(pb.removeFromTop(Helpers::ParameterHeight));
					pb.removeFromTop(Helpers::ParameterMargin);
				}
					
			}
			if(!modOutputs.isEmpty())
			{
				auto mb = b.removeFromRight(Helpers::ModulationOutputWidth);
				
				for(auto m: modOutputs)
				{
					m->setBounds(mb.removeFromTop(Helpers::ModulationOutputHeight));
					mb.removeFromTop(Helpers::ParameterMargin);
				}
			}
		}

		ValueTree data;
		UndoManager* um;
		OwnedArray<ParameterComponent> parameters;
		OwnedArray<ModOutputComponent> modOutputs;
		ScopedPointer<Component> dummyBody;
	};

	void toggle(const Identifier& id)
	{
		auto v = (bool)data[id];
		data.setProperty(id, !v, um);
	}

	NodeComponent(Lasso* l, const ValueTree& v, UndoManager* um_, bool useSignal) :
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

		rebuildDefaultParametersAndOutputs();

		colourListener.setCallback(data, { PropertyIds::NodeColour }, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onColour));

		
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
		LOAD_EPATH_IF_URL("goto", ColumnIcons::openWorkspaceIcon);

		return p;
	}

	void onPositionUpdate(const Identifier&, const var&)
	{
		auto pos = Helpers::getPosition(data);
		DBG(Helpers::getHeaderTitle(getValueTree()) + ": " + pos.toString());

		setTopLeftPosition(Helpers::getPosition(data));
	}

	virtual void onFold(const Identifier& id, const var& newValue);

	void rebuildDefaultParametersAndOutputs()
	{
		parameters.clear();
		modOutputs.clear();

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

		resized();
	}

	void resized() override
	{
		auto b = getLocalBounds();
		header.setBounds(b.removeFromTop(Helpers::HeaderHeight));

		if (hasSignal)
			b.removeFromTop(Helpers::SignalHeight);

		Rectangle<int> pb;

		auto ParameterWidth = Helpers::ParameterWidth;

		if (getValueTree()[PropertyIds::Folded])
			ParameterWidth -= Helpers::ParameterHeight;

		if(!parameters.isEmpty())
			pb = b.removeFromLeft(Helpers::ParameterMargin + ParameterWidth);

		pb.removeFromLeft(Helpers::ParameterMargin);

		auto mb = b.removeFromRight(Helpers::ModulationOutputWidth);

		for (auto p: parameters)
		{
			p->setBounds(pb.removeFromTop(getParameterHeight()));
			pb.removeFromTop(Helpers::ParameterMargin);
		}

		for (auto m : modOutputs)
		{
			m->setBounds(mb.removeFromTop(Helpers::ModulationOutputHeight));
			mb.removeFromTop(Helpers::ParameterMargin);
		}
	}

	ValueTree getValueTree() const override { return data; }

	UndoManager* getUndoManager() { return um; }

	const bool hasSignal;

	int getParameterHeight() const { return Helpers::ParameterHeight; }

	void setFixSize(Rectangle<int> extraBounds)
	{
		int x = 0;

		auto ParameterWidth = Helpers::ParameterWidth;

		if (getValueTree()[PropertyIds::Folded])
			ParameterWidth -= Helpers::ParameterHeight;
		
		if (!parameters.isEmpty())
			x += Helpers::ParameterMargin + ParameterWidth;

		int y = Helpers::HeaderHeight;

		

		if(false && !hasSignal)
			x += extraBounds.getWidth();

		if (!modOutputs.isEmpty())
			x += Helpers::ModulationOutputWidth;

		if (hasSignal)
			y += Helpers::SignalHeight;

		if (Helpers::hasRoutableSignal(data))
			y += Helpers::SignalHeight;

		int bodyHeight = 0;

		auto parameterHeight = parameters.size() * (Helpers::ParameterMargin + getParameterHeight());
		auto modHeight = modOutputs.size() * (Helpers::ModulationOutputHeight + Helpers::ParameterMargin);
		
		parameterHeight = jmax(parameterHeight, modHeight);

		if(true || hasSignal)
			bodyHeight = extraBounds.getHeight() + parameterHeight;
		else
			bodyHeight = jmax(extraBounds.getHeight(), parameterHeight);

		x = jmax(x, GLOBAL_FONT().getStringWidth(Helpers::getHeaderTitle(getValueTree())) + 20 + 2 * Helpers::HeaderHeight);

		x = jmax(x, extraBounds.getWidth());

		if(Helpers::hasRoutableSignal(data))
			x = jmax(x, Helpers::ParameterWidth + 2 * Helpers::ParameterMargin);

		Rectangle<int> nb(getPosition(), getPosition().translated(x, y + bodyHeight));

		Helpers::updateBounds(getValueTree(), nb, um);

		setSize(x, y + bodyHeight);
		resized();
	}

	virtual Component* createPopupComponent() { return new PopupComponent(getValueTree(), um); }

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

struct ContainerComponentBase
{
	virtual ~ContainerComponentBase() = default;

	NodeComponent& asNodeComponent() { return *dynamic_cast<NodeComponent*>(this); }

	virtual CablePinBase* createOutsideParameterComponent(const ValueTree& source) = 0;
	
	void rebuildOutsideParameters()
	{
		auto before = outsideParameters.size();

		outsideParameters.clear();

		auto pTrees = ParameterHelpers::getAutomatedChildParameters(asNodeComponent().getValueTree());

		auto isLocked = (bool)asNodeComponent().getValueTree()[PropertyIds::Locked];

		for (auto p : pTrees)
		{
			auto source = ParameterHelpers::getConnection(p);

			if (!isLocked && !source[UIPropertyIds::HideCable])
				continue;

			auto conParent = ParameterHelpers::findConnectionParent(source);

			auto found = false;

			for(auto op: outsideParameters)
			{
				if(op->data == conParent)
				{
					found = true;
					break;
				}
			}

			if(found)
				continue;

			if(auto np = createOutsideParameterComponent(source))
			{
				outsideParameters.add(np);
				asNodeComponent().addAndMakeVisible(np);
			}
		}

		onOutsideParameterChange(before, outsideParameters.size());
	}

	virtual void onOutsideParameterChange(int before, int now)
	{
		asNodeComponent().resized();
		asNodeComponent().repaint();
	}

	void drawOutsideLabel(Graphics& g)
	{
		if (!outsideLabel.isEmpty() && !outsideParameters.isEmpty())
		{
			g.setFont(GLOBAL_FONT());
			g.setColour(Colours::white.withAlpha(0.3f));
			g.drawText("External Connections", outsideLabel.toFloat(), Justification::centred);
		}
	}

	int getNumOutsideParameters() const { return outsideParameters.size(); }

	CablePinBase* getOutsideParameter(int index) const { return outsideParameters[index]; }

	CablePinBase* getOutsideParameter(const ValueTree& pTree)
	{
		jassert(pTree.getType() == PropertyIds::Parameter ||
			pTree.getType() == PropertyIds::ModulationTargets ||
			pTree.getType() == PropertyIds::SwitchTarget ||
			ParameterHelpers::isRoutingSendNode(pTree));

		for (auto op : outsideParameters)
		{
			if (op->data == pTree)
				return op;
		}

		return nullptr;
	}

	void positionOutsideParameters(int yOffset)
	{
		if (!outsideParameters.isEmpty())
		{
			outsideLabel = { 0.0f, (float)yOffset, (float)(Helpers::ParameterMargin + Helpers::ParameterWidth), 20.0f };
			yOffset += 25;
		}

		for (auto op : outsideParameters)
		{
			yOffset += Helpers::ParameterMargin;
			op->setTopLeftPosition(Helpers::ParameterMargin, yOffset);
			yOffset += op->getHeight();
		}
	}

private:

	OwnedArray<CablePinBase> outsideParameters;

	Rectangle<float> outsideLabel;

	JUCE_DECLARE_WEAK_REFERENCEABLE(ContainerComponentBase);
};

}
