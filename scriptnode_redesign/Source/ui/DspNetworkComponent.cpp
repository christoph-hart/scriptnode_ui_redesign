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




DspNetworkComponent::DspNetworkComponent(PooledUIUpdater* updater, ZoomableViewport& zp, const ValueTree& networkTree, const ValueTree& container) :
	CableHolder(networkTree),
	Lasso(updater),
	LODManager(zp),
	data(networkTree)
{
	Helpers::migrateFeedbackConnections(data, true, nullptr);
	Helpers::updateChannelCount(data, false, nullptr);

	valuetree::Helpers::forEach(networkTree, [&](ValueTree& n)
		{
			if (n.getType() == PropertyIds::Node)
				n.setProperty(UIPropertyIds::CurrentRoot, n == container, &um);

			return false;
		});

	setWantsKeyboardFocus(true);

	addAndMakeVisible(lasso);

	lasso.setColour(LassoComponent<NodeComponent*>::ColourIds::lassoFillColourId, Colour(SIGNAL_COLOUR).withAlpha(0.05f));
	lasso.setColour(LassoComponent<NodeComponent*>::ColourIds::lassoOutlineColourId, Colour(SIGNAL_COLOUR));

	auto rootContainer = container;
	jassert(rootContainer.getType() == PropertyIds::Node);

	auto sortProcessNodes = !rootContainer.hasProperty(UIPropertyIds::width);

	Helpers::fixOverlap(rootContainer, &um, sortProcessNodes);

	addAndMakeVisible(rootComponent = new ContainerComponent(this, rootContainer, &um));

	rootSizeListener.setCallback(rootContainer,
		{ UIPropertyIds::width, UIPropertyIds::height },
		Helpers::UIMode,
		VT_BIND_PROPERTY_LISTENER(onContainerResize));


	foldListener.setCallback(rootContainer, { PropertyIds::Folded }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onFold));

	Helpers::fixOverlap(rootContainer, &um, false);

	auto b = Helpers::getBounds(rootContainer, false);
	setSize(b.getWidth(), b.getHeight());
}

void DspNetworkComponent::onFold(const ValueTree& v, const Identifier& id)
{
	SafeAsyncCall::call<DspNetworkComponent>(*this, [](DspNetworkComponent& d)
		{
			d.rebuildCables();
		});

	return;

	for (auto c : cables)
	{
		auto sourceVisible = !Helpers::isFoldedRecursive(c->src->getNodeTree());
		auto targetVisible = !Helpers::isFoldedRecursive(c->dst->getNodeTree());

		c->setVisible(sourceVisible && targetVisible);
	}
}

void DspNetworkComponent::onContainerResize(const Identifier& id, const var& newValue)
{
	auto b = Helpers::getBounds(rootComponent->getValueTree(), false);
	setSize(b.getWidth(), b.getHeight());
}

bool DspNetworkComponent::isEditModeEnabled(const MouseEvent& e)
{
	auto root = e.eventComponent->findParentComponentOfClass<DspNetworkComponent>();

	if (root != nullptr && root->editMode)
		return true;

	return e.mods.isCommandDown();
}

void DspNetworkComponent::paint(Graphics& g)
{
	g.fillAll(Colour(0xFF222222));

	if (editMode)
		GlobalHiseLookAndFeel::draw1PixelGrid(g, this, getLocalBounds());

	if (!currentBuildPath.isEmpty())
	{
		g.setColour(Colour(SIGNAL_COLOUR));
		g.strokePath(currentBuildPath, PathStrokeType(1.0f));
		g.setColour(Colour(SIGNAL_COLOUR).withAlpha(0.1f));
		g.fillPath(currentBuildPath);
	}

	for (const auto& p : snapshotPositions)
	{
		Path p1, p2, p3, p4;

		auto tl = p.second.viewport.getTopLeft().toFloat();
		auto tr = p.second.viewport.getTopRight().toFloat();
		auto bl = p.second.viewport.getBottomLeft().toFloat();
		auto br = p.second.viewport.getBottomRight().toFloat();

		const float w = 15.0f;

		p1.startNewSubPath(tl.translated(0, w));
		p1.lineTo(tl);
		p1.lineTo(tl.translated(w, 0.0f));

		p2.startNewSubPath(tr.translated(0, w));
		p2.lineTo(tr);
		p2.lineTo(tr.translated(-w, 0.0f));

		p3.startNewSubPath(bl.translated(0, -w));
		p3.lineTo(bl);
		p3.lineTo(bl.translated(w, 0.0f));

		p4.startNewSubPath(br.translated(0, -w));
		p4.lineTo(br);
		p4.lineTo(br.translated(-w, 0.0f));

		PathStrokeType ps(2.0f);

		g.setColour(Colours::white.withAlpha(0.25f));

		g.strokePath(p1.createPathWithRoundedCorners(2.0f), ps);
		g.strokePath(p2.createPathWithRoundedCorners(2.0f), ps);
		g.strokePath(p3.createPathWithRoundedCorners(2.0f), ps);
		g.strokePath(p4.createPathWithRoundedCorners(2.0f), ps);

		g.setFont(GLOBAL_FONT());

		g.drawText(String(&p.first, 1), p.second.viewport.toFloat().reduced(4.0f, 2.0f), Justification::topRight);
		g.drawText(String(&p.first, 1), p.second.viewport.toFloat().reduced(4.0f, 2.0f), Justification::topLeft);
		g.drawText(String(&p.first, 1), p.second.viewport.toFloat().reduced(4.0f, 2.0f), Justification::bottomLeft);
		g.drawText(String(&p.first, 1), p.second.viewport.toFloat().reduced(4.0f, 2.0f), Justification::bottomRight);
	}
}

void DspNetworkComponent::resized()
{
	rebuildCables();
}

void DspNetworkComponent::mouseMove(const MouseEvent& e)
{
	pos = e.getPosition();
}

void DspNetworkComponent::mouseDown(const MouseEvent& e)
{
	if (e.mods.isX1ButtonDown())
	{
		NetworkParent::getNetworkParent(this)->getViewUndoManager()->undo();
		return;
	}
	if (e.mods.isX2ButtonDown())
	{
		NetworkParent::getNetworkParent(this)->getViewUndoManager()->redo();
		return;
	}

	CHECK_MIDDLE_MOUSE_DOWN(e);

	checkLassoEvent(e, SelectableComponent::LassoState::Down);
}

void DspNetworkComponent::mouseDrag(const MouseEvent& e)
{
	CHECK_MIDDLE_MOUSE_DRAG(e);

	if (ZoomableViewport::checkDragScroll(e, false))
		return;

	checkLassoEvent(e, SelectableComponent::LassoState::Drag);
}

void DspNetworkComponent::mouseDoubleClick(const MouseEvent& e)
{
	if (auto c = rootComponent->getInnerContainer(this, e.getPosition(), nullptr))
	{
		auto pos = c->getLocalPoint(this, e.position);
		c->renameGroup(pos);
	}
}

void DspNetworkComponent::mouseUp(const MouseEvent& e)
{
	CHECK_MIDDLE_MOUSE_UP(e);

	if (ZoomableViewport::checkDragScroll(e, true))
		return;

	checkLassoEvent(e, SelectableComponent::LassoState::Up);

	if (!e.mouseWasDraggedSinceMouseDown())
	{
		if (auto c = rootComponent->getInnerContainer(this, e.getPosition(), nullptr))
		{
			auto pos = c->getLocalPoint(this, e.position);

			if (!e.mods.isCommandDown())
				selection.deselectAll();

			if (c->selectGroup(pos))
				return;
		}

		selection.deselectAll();
	}
}

void DspNetworkComponent::toggleEditMode()
{
	editMode = !editMode;
	repaint();
}

bool DspNetworkComponent::performAction(Action a)
{
	switch (a)
	{
	case Action::Open:
		break;
	case Action::DeselectAll:
		getLassoSelection().deselectAll();
		return true;
	case Action::Cut:
		cutSelection();
		return true;
	case Action::Copy:
		copySelection();
		return true;
	case Action::SetColour:
		colourSelection();
		return true;
	case Action::Paste:
	{
		auto currentContainer = rootComponent->getInnerContainer(this, pos, nullptr);
		auto pastePos = currentContainer->getLocalPoint(this, pos);
		pasteSelection(pastePos);
		return true;
	}
	case Action::Duplicate:
		duplicateSelection();
		return true;
	case Action::GroupSelection:
		groupSelection();
		return true;
	case Action::Delete:
		deleteSelection();
		return true;
	case Action::ToggleFold:
		for(auto n: createTreeListFromSelection(PropertyIds::Node))
			n.setProperty(PropertyIds::Folded, !n[PropertyIds::Folded], &um);
		return true;
	case Action::Rename:
		if(auto f = dynamic_cast<NodeComponent*>(selection.getSelectedItem(0).get()))
		{
			f->rename();
		}
		return true;
	case Action::EditXML:
	{
		auto treeSelection = createTreeListFromSelection(PropertyIds::Node);

		if(treeSelection.getFirst().isValid())
		{
			NetworkParent::PopupCodeEditor::show(dynamic_cast<Component*>(selection.getSelectedItem(0).get()), treeSelection.getFirst(), Identifier(), &um);
		}

		return true;
	}
	case Action::ToggleVertical:
		for (auto s : createTreeListFromSelection(PropertyIds::Node))
			Helpers::setNodeProperty(s, PropertyIds::IsVertical, !Helpers::getNodeProperty(s, PropertyIds::IsVertical, true), &um);
		return true;
	case Action::HideCable:
	{
		auto treeSelection = createTreeListFromSelection(PropertyIds::Connection);

		for (auto s : treeSelection)
			s.setProperty(UIPropertyIds::HideCable, !s[UIPropertyIds::HideCable], &um);

		MessageManager::callAsync([this, treeSelection]()
			{
				getLassoSelection().deselectAll();

				callRecursive<CableComponent>(this, [&](CableComponent* c)
					{
						if (treeSelection.contains(c->getValueTree()))
							getLassoSelection().addToSelection(c);

						return false;
					});
			});

		return true;
	}
	case Action::ShowMap:
	{
		auto p = NetworkParent::getNetworkParent(this);
		auto lp = p->asComponent()->getLocalPoint(this, pos);
		p->showMap(rootComponent->getValueTree(), getCurrentViewPosition(), lp);

		return true;
	}
	case Action::ToggleEdit:
		toggleEditMode();
		return true;
	case Action::CollapseContainer:

		for (auto c : createTreeListFromSelection(PropertyIds::Node))
			c.setProperty(PropertyIds::Locked, !c[PropertyIds::Locked], &um);

		return true;
	case Action::CreateNew:
	{
		if (auto first = selection.getItemArray().getFirst())
		{
			if (auto nc = dynamic_cast<NodeComponent*>(first.get()))
			{
				BuildHelpers::CreateData cd;

				Path buildPath;

				Point<int> pos;

				if (auto c = dynamic_cast<ContainerComponent*>(nc))
				{
					cd.containerToInsert = c->getValueTree();
					cd.pointInContainer = {};
					cd.signalIndex = c->childNodes.size();

					if (auto ab = c->addButtons.getLast())
					{
						buildPath = ab->hitzone;
						pos = getLocalPoint(ab, Point<int>());
						buildPath.applyTransform(AffineTransform::translation(pos.toFloat()));
					}
				}
				else
				{
					auto container = nc->findParentComponentOfClass<ContainerComponent>();

					cd.containerToInsert = container->getValueTree();
					cd.pointInContainer = nc->getBoundsInParent().getTopRight().translated(Helpers::NodeMargin / 2, 0);

					pos = getLocalPoint(container, cd.pointInContainer);

					NetworkParent::scrollToRectangle(this, Rectangle<int>(pos, pos).withSizeKeepingCentre(400, 32), true);

					if (nc->hasSignal)
					{
						cd.signalIndex = nc->getValueTree().getParent().indexOf(nc->getValueTree()) + 1;

						if (auto ab = container->addButtons[cd.signalIndex])
						{
							buildPath = ab->hitzone;

							auto pos = getLocalPoint(ab, Point<float>());
							buildPath.applyTransform(AffineTransform::translation(pos));
						}
					}
					else
					{
						cd.source = nc->getFirstModOutput();
					}
				}

				showCreateNodePopup(pos, nc, buildPath, cd);
			}
			if (auto cc = dynamic_cast<CableComponent*>(first.get()))
			{
				auto pos = cc->getBoundsInParent().getCentre();

				auto delta = cc->getPosition();

				auto buildPath = cc->p;
				buildPath.applyTransform(AffineTransform::translation(delta));

				BuildHelpers::CreateData cd;

				auto container = cc->src->findParentComponentOfClass<ContainerComponent>();

				auto sourceBounds = Helpers::getBounds(cc->src->getNodeTree(), true);

				auto newX = sourceBounds.getRight() + Helpers::NodeMargin;
				auto newY = sourceBounds.getY();

				cd.containerToInsert = container->getValueTree();
				cd.pointInContainer = { newX, newY };
				cd.source = cc->src;
				cd.target = cc->dst;
				cd.isCableNode = true;

				showCreateNodePopup(pos, cc, buildPath, cd);
			}
		}
		else
		{
			if (auto c = rootComponent->getInnerContainer(this, pos, nullptr))
			{
				BuildHelpers::CreateData cd;

				Path buildPath;

				auto w = Helpers::ContainerParameterWidth + Helpers::ModulationOutputWidth + 2 * Helpers::ParameterMargin;
				auto h = Helpers::HeaderHeight + Helpers::ParameterMargin * 2 + Helpers::ParameterHeight;

				buildPath.addRoundedRectangle(Rectangle<int>(pos, pos).withSizeKeepingCentre(w, h).toFloat(), 3.0f);
				
				cd.containerToInsert = c->getValueTree();
				cd.pointInContainer = c->getLocalPoint(this, buildPath.getBounds().getPosition().toInt());
				cd.signalIndex = -1;
				cd.source = nullptr;
				cd.target = nullptr;
				cd.isCableNode = true;

				auto p = dynamic_cast<Component*>(NetworkParent::getNetworkParent(this));

				showCreateNodePopup(pos, c, buildPath, cd);
			}

			//showCreateConnectionPopup(pos);
		}

		return true;
	}
	case Action::AddComment:

		if (selection.getNumSelected() == 0)
		{
			if (auto c = rootComponent->getInnerContainer(rootComponent, pos, nullptr))
			{
				auto cTree = c->getValueTree().getOrCreateChildWithName(UIPropertyIds::Comments, &um);

				auto lp = c->getLocalPoint(this, pos);

				ValueTree nc(PropertyIds::Comment);

				nc.setProperty(PropertyIds::Comment, "Add comment", nullptr);
				nc.setProperty(UIPropertyIds::CommentOffsetX, lp.getX(), nullptr);
				nc.setProperty(UIPropertyIds::CommentOffsetY, lp.getY(), nullptr);
				nc.setProperty(UIPropertyIds::CommentWidth, 500, nullptr);

				cTree.addChild(nc, -1, &um);
			}
		}
		else
		{
			for (auto s : selection.getItemArray())
			{
				String t = "Add comment...";

				if (s != nullptr && s->isNode())
				{
					auto desc = db.getDescriptions()[s->getValueTree()[PropertyIds::FactoryPath]];

					if (desc.isNotEmpty())
						t = desc;
				}

				s->getValueTree().setProperty(PropertyIds::Comment, t, &um);
			}
		}

		return true;
	case Action::Undo:
		um.undo();
		return true;
	case Action::Redo:
		um.redo();
		return true;
	case Action::AutoLayout:
		if (selection.getNumSelected() == 0)
		{
			Helpers::resetLayout(rootComponent->getValueTree(), &um);
		}
		else
		{
			for (auto s : selection.getItemArray())
			{
				if (s != nullptr && s->isNode())
					Helpers::resetLayout(s->getValueTree(), &um);
			}
		}



		return true;
	case Action::AlignTop:
	{

		LayoutTools::alignHorizontally(createTreeListFromSelection(PropertyIds::Node), &um);

		Array<LayoutTools::CableData> cableList;

		for (auto s : selection.getItemArray())
		{
			if (auto c = dynamic_cast<CableComponent*>(s.get()))
			{
				LayoutTools::CableData cd;
				cd.con = s->getValueTree();
				cd.s = getLocalPoint(c, c->s);
				cd.e = getLocalPoint(c, c->e);
				cableList.add(cd);
			}
		}

		LayoutTools::alignCables(cableList, &um);
		return true;
	}
	case Action::AlignLeft:
		LayoutTools::alignVertically(createTreeListFromSelection(PropertyIds::Node), &um);
		return true;
	case Action::DistributeHorizontally:
		LayoutTools::distributeHorizontally(createTreeListFromSelection(PropertyIds::Node), &um);
		LayoutTools::distributeCableOffsets(createTreeListFromSelection(PropertyIds::Connection), &um);
		return true;
	case Action::DistributeVertically:
		LayoutTools::distributeVertically(createTreeListFromSelection(PropertyIds::Node), &um);
		return true;
	case Action::numActions:
		break;
	default:
		break;
	}
}

bool DspNetworkComponent::keyPressed(const KeyPress& k)
{
	if (k.getKeyCode() == 'D' && k.getModifiers().isCommandDown())
		return performAction(Action::Duplicate);
	if (k.getKeyCode() == 'Z' && k.getModifiers().isCommandDown())
		return performAction(Action::Undo);
	if (k.getKeyCode() == 'C' && k.getModifiers().isCommandDown())
		return performAction(Action::Copy);
	if (k.getKeyCode() == 'X' && k.getModifiers().isCommandDown())
		return performAction(Action::Cut);
	if (k.getKeyCode() == KeyPress::F4Key)
		return performAction(Action::ToggleEdit);
	if (k.getKeyCode() == KeyPress::escapeKey)
		return performAction(Action::DeselectAll);
	if (k.getKeyCode() == '#' && k.getModifiers().isCommandDown())
		return performAction(Action::AddComment);
	if (k.getKeyCode() == 'V' && k.getModifiers().isCommandDown())
		return performAction(Action::Paste);
	if (k.getKeyCode() == 'R' && k.getModifiers().isCommandDown())
		return performAction(Action::AutoLayout);
	if (k.getKeyCode() == 'Y' && k.getModifiers().isCommandDown())
		return performAction(Action::Redo);
	if (k.getKeyCode() == 'G' && k.getModifiers().isCommandDown())
		return performAction(Action::GroupSelection);
	if (k.getKeyCode() == 'L' && k.getModifiers().isCommandDown())
		return performAction(Action::CollapseContainer);
	if (k.getKeyCode() == KeyPress::deleteKey)
		return performAction(Action::Delete);
	if (k.getKeyCode() == 'N')
		return performAction(Action::CreateNew);
	if (k.getKeyCode() == 'P')
	{
		showCreateConnectionPopup(pos);
		return true;
	}
	if(k.getKeyCode() == 'F')
		return performAction(Action::ToggleFold);
	if(k == KeyPress::F2Key)
		return performAction(Action::Rename);
	if(k.getKeyCode() == 'J')
		return performAction(Action::EditXML);
	if (k.getKeyCode() == 'M')
		return performAction(Action::ShowMap);
	if (k.getKeyCode() == 'H')
		return performAction(Action::HideCable);

	if (k.getKeyCode() == '1' || k.getKeyCode() == '2' || k.getKeyCode() == '3' || k.getKeyCode() == '4')
	{
		auto store = k.getModifiers().isCommandDown();

		if (store)
		{
			if (snapshotPositions.find(k.getKeyCode()) != snapshotPositions.end())
			{
				snapshotPositions.erase(k.getKeyCode());
				repaint();
				return true;
			}

			auto s = getCurrentViewPosition();



			snapshotPositions[(char)k.getKeyCode()] = { data, s.reduced(20) };
			repaint();
		}
		else
		{
			if (snapshotPositions.find(k.getKeyCode()) != snapshotPositions.end())
			{
				snapshotPositions.at(k.getKeyCode()).restore(*findParentComponentOfClass<ZoomableViewport>(), &um);

				//->zoomToRectangle(snapshotPositions[(char)k.getKeyCode()]);
			}
		}

		return true;
	}

	if (k.getKeyCode() == KeyPress::leftKey || k.getKeyCode() == KeyPress::rightKey || k.getKeyCode() == KeyPress::downKey || k.getKeyCode() == KeyPress::upKey)
	{
		return navigateSelection(k);
	}

	return false;
}

Rectangle<int> DspNetworkComponent::getCurrentViewPosition() const
{
	auto fullBounds = getLocalBounds();
	fullBounds.removeFromTop(Helpers::HeaderHeight);
	auto parentBounds = getParentComponent()->getLocalBounds();
	parentBounds = getLocalArea(getParentComponent(), parentBounds);
	auto s = parentBounds.getIntersection(fullBounds.reduced(20));
	return s;
}

void DspNetworkComponent::findLassoItemsInArea(Array<SelectableComponent::WeakPtr>& itemsFound, const Rectangle<int>& area)
{
	callRecursive<SelectableComponent>(this, [&](SelectableComponent* nc)
		{
			auto asComponent = dynamic_cast<Component*>(nc);

			if (!asComponent->isShowing())
				return false;

			if (auto c = dynamic_cast<CableBase*>(nc))
			{
				if (!itemsFound.contains(nc))
				{
					auto la = c->getLocalArea(this, area.toFloat());

					if (c->p.intersectsLine({ la.getTopLeft(), la.getBottomRight() }))
					{
						itemsFound.add(nc);
					}
				}
			}
			else
			{
				auto nb = getLocalArea(asComponent, asComponent->getLocalBounds());

				if (area.intersects(nb) && !nb.contains(area))
				{
					auto p = asComponent->findParentComponentOfClass<SelectableComponent>();

					while (p != nullptr)
					{
						itemsFound.removeAllInstancesOf(p);
						asComponent = dynamic_cast<Component*>(p);
						p = asComponent->findParentComponentOfClass<SelectableComponent>();
					}

					itemsFound.addIfNotAlreadyThere(nc);
				}
			}

			return false;
		});
}

void DspNetworkComponent::setIsDragged(NodeComponent* nc)
{
	DraggedNode dn;
	dn.originalParent = dynamic_cast<ContainerComponent*>(nc->getParentComponent());
	dn.node = nc;

	dn.removeFromParent(*this);

#if 0
	auto currentBounds = nc->getBoundsInParent();

	currentlyDraggedComponents.add(nc);
	currentParent->removeChildComponent(nc);
	addChildComponent(nc);

	currentParent->cables.updatePins(*currentParent);

	nc->setAlpha(0.5f);

	auto rootBounds = getLocalArea(currentParent, currentBounds);
	nc->setBounds(rootBounds);
#endif
}

void DspNetworkComponent::clearDraggedComponents()
{
	for(auto& c: currentlyDraggedComponents)
		c.addToParent(*this);

	currentlyDraggedComponents.clear();

	if(currentlyHoveredContainer != nullptr)
		currentlyHoveredContainer->repaint();

	currentlyHoveredContainer = nullptr;
}

void DspNetworkComponent::resetDraggedBounds()
{
	if (currentlyHoveredContainer != nullptr)
	{
		for (auto nc : currentlyDraggedComponents)
		{
			auto containerBounds = currentlyHoveredContainer->getLocalArea(this, nc.node->getBoundsInParent());
			Helpers::updateBounds(nc.node->getValueTree(), containerBounds, &um);
		}
	}
}

void DspNetworkComponent::showPopup(Component* popupComponent, Component* target, Rectangle<int> specialBounds /*= {}*/)
{
	NetworkParent::getNetworkParent(target)->showPopup(popupComponent, target, specialBounds);
}

void DspNetworkComponent::switchRootNode(Component* c, ValueTree newRoot)
{
	if (auto vp = c->findParentComponentOfClass<ZoomableViewport>())
	{
		auto updater = c->findParentComponentOfClass<Lasso>()->getUpdater();
		auto root = valuetree::Helpers::findParentWithType(newRoot, PropertyIds::Network);
		auto n = new DspNetworkComponent(updater, *vp, root, newRoot);
		vp->setNewContent(n, nullptr);
	}
}

struct DspNetworkComponent::CreateConnectionPopup : public Component,
													public PathFactory
{
	struct Item : public Component
	{
		static constexpr int Height = 24;

		Item(CablePinBase::WeakPtr src, CreateConnectionPopup& parent) :
			source(src),
			name(source->getSourceDescription()),
			target(parent.createPath("target")),
			c(ParameterHelpers::getParameterColour(src->data)),
			moreButton("more", nullptr, parent)
		{
			addAndMakeVisible(moreButton);
			parent.content.addAndMakeVisible(this);

			auto pos = parent.parent.pos;

			auto container = parent.parent.rootComponent->getInnerContainer(&parent.parent, pos, nullptr);

			if (container != nullptr)
				pos = container->getLocalPoint(&parent.parent, pos);

			auto um = &parent.parent.um;

			moreButton.onClick = [this, pos, um]()
			{
				auto pTree = source->data;

				PopupLookAndFeel plaf;
				PopupMenu m;
				m.setLookAndFeel(&plaf);

				m.addSectionHeader("Parameter Management");
				m.addItem(1, "Create local instance at position");
				m.addItem(2, "Hide all connections");
				m.addItem(3, "Edit parameter range");
				m.addSeparator();
				m.addItem(4, "Rename parameter");
				m.addItem(5, "Delete parameter");

				auto result = m.show();

				jassert(pTree.getType() == PropertyIds::Parameter);	

				if(result == 1) // Breakout
				{
					pTree.setProperty(UIPropertyIds::x, pos.getX(), um);
					pTree.setProperty(UIPropertyIds::y, pos.getY(), um);
				}
				if(result == 2) // HIDE
				{
				}
				if(result == 3) // Edit parameter range
				{

				}
				if(result == 4) // Rename
				{

				}
				if(result == 5) // Delete
				{

				}
			};

			auto w = GLOBAL_BOLD_FONT().getStringWidth(name) + 20 + Height;

			setBounds(0, parent.items.size() * Height, w, Height);
			parent.items.add(this);
			parent.content.setSize(w, parent.items.size() * Height);
			setMouseCursor(MouseCursor::CrosshairCursor);
			setRepaintsOnMouseActivity(true);
		}

		void mouseDown(const MouseEvent& e) override
		{
			if (source != nullptr)
				source->mouseDown(e.getEventRelativeTo(source));
		}

		void mouseDrag(const MouseEvent& e) override
		{
			if (source != nullptr)
				source->mouseDrag(e.getEventRelativeTo(source));
		}

		void mouseUp(const MouseEvent& e) override
		{
			if (source != nullptr)
				source->mouseUp(e.getEventRelativeTo(source));
		}

		void paint(Graphics& g) override
		{
			auto b = getLocalBounds().toFloat();

			g.setColour(Colours::black.withAlpha(0.2f));
			g.fillRoundedRectangle(b.reduced(1.0f), 3.0f);

			float alpha = 0.7f;

			if (isMouseOver())
				alpha += 0.1f;

			if (isMouseOverOrDragging())
				alpha += 0.1f;


			g.setColour(c.withAlpha(alpha));
			g.fillPath(target);
			g.setFont(GLOBAL_FONT());
			g.drawText(name, tb, Justification::left);
		}

		void resized() override
		{
			auto b = getLocalBounds().toFloat();

			moreButton.setBounds(b.removeFromRight(15).toNearestInt().reduced(3));

			if (!b.isEmpty())
			{
				scalePath(target, b.removeFromRight(b.getHeight()).reduced(3.0f));
				b.removeFromLeft(5.0f);
				tb = b;
				repaint();
			}
		}

		Colour c;
		CablePinBase::WeakPtr source;
		Rectangle<float> tb;
		Path target;
		String name;
		HiseShapeButton moreButton;
	};

	Path createPath(const String& url) const override
	{
		Path p;
		LOAD_EPATH_IF_URL("target", ColumnIcons::targetIcon);
		LOAD_EPATH_IF_URL("more", SampleToolbarIcons::more);
		LOAD_EPATH_IF_URL("add", HiBinaryData::ProcessorEditorHeaderIcons::addIcon);
		return p;
	}

	void paint(Graphics& g) override
	{
		auto b = getLocalBounds();
		g.setColour(Colour(0xEE252525));
		g.fillRoundedRectangle(b.reduced(3).toFloat(), 3.0f);
	}

	void resized() override
	{
		auto b = getLocalBounds();

		auto bottom = b.removeFromBottom(24);
		b.removeFromBottom(10);

		addParameter.setBounds(bottom.removeFromRight(bottom.getHeight()).reduced(4));
		newParameterId.setBounds(bottom);

		vp.setBounds(b);

		auto w = b.getWidth() - vp.getScrollBarThickness();

		for (auto i : items)
			i->setSize(w, Item::Height);

		content.setSize(w, content.getHeight());
	}

	Component content;

	CreateConnectionPopup(DspNetworkComponent& parent_, ValueTree containerTree) :
		parent(parent_),
		addParameter("add", nullptr, *this),
	    containerData(containerTree)
	{

		setName("Parameter Management");
		addAndMakeVisible(vp);
		addAndMakeVisible(newParameterId);
		addAndMakeVisible(addParameter);

		auto pos = parent.pos;

		auto container = parent.rootComponent->getInnerContainer(&parent, pos, nullptr);

		if (container != nullptr)
			pos = container->getLocalPoint(&parent, pos);

		addParameter.onClick = [this, pos]()
		{
			auto newId = newParameterId.getText();

			auto pTree = containerData.getChildWithName(PropertyIds::Parameters);

			if(pTree.getChildWithProperty(PropertyIds::ID, newId).isValid())
			{
				// error
				jassertfalse;
			}

			ValueTree np(PropertyIds::Parameter);
			
			ValueTree con(PropertyIds::Connections);

			np.setProperty(PropertyIds::ID, newId, nullptr);
			np.setProperty(PropertyIds::Value, 0.0, nullptr);
			np.addChild(con, -1, nullptr);

			RangeHelpers::storeDoubleRange(np, { 0.0, 1.0 }, nullptr, RangeHelpers::IdSet::scriptnode);
			
			if (pos.getX() > Helpers::ContainerParameterWidth)
			{
				np.setProperty(UIPropertyIds::x, pos.getX(), nullptr);
				np.setProperty(UIPropertyIds::y, pos.getY(), nullptr);
			}

			pTree.addChild(np, -1, &parent.um);

		};

		GlobalHiseLookAndFeel::setTextEditorColours(newParameterId);

		vp.setViewedComponent(&content, false);
		sf.addScrollBarToAnimate(vp.getVerticalScrollBar());

		std::map<String, CablePinBase::WeakPtr> sources;

		callRecursive<CablePinBase>(&parent, [&](CablePinBase* p)
		{
			if (p->isDraggable())
				sources[p->getSourceDescription()] = p;

			return false;
		});

		auto w = 200;

		for (auto s : sources)
		{
			new Item(s.second, *this);
			w = jmax(w, items.getLast()->getWidth());
		}

		setSize(w, jmin(items.size() * Item::Height, 300) + 34);
	}

	ValueTree containerData;
	OwnedArray<Item> items;
	ScrollbarFader sf;
	DspNetworkComponent& parent;
	Viewport vp;
	TextEditor newParameterId;
	HiseShapeButton addParameter;
};

struct DspNetworkComponent::CreateNodePopup : public Component,
	public TextEditorWithAutocompleteComponent,
	public ZoomableViewport::ZoomListener
{
	struct Laf : public GlobalHiseLookAndFeel,
				 public TextEditorWithAutocompleteComponent::LookAndFeelMethods
	{
		void drawButtonBackground(Graphics& g, Button& b, const Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
		{
			Rectangle<float> r = b.getLocalBounds().toFloat().reduced(1.0f);

			Path p;

			auto f = b.getConnectedEdgeFlags();

			auto left = (f & Button::ConnectedOnLeft) != 0;
			auto right = (f & Button::ConnectedOnRight) != 0;

			p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), r.getHeight() / 2.0f, r.getHeight() * 0.5f, !right, !left, !right, !left);
			g.setColour(b.findColour(TextButton::ColourIds::buttonColourId).withAlpha(1.0f));

			if (b.getToggleState())
				g.fillPath(p);
			else
				g.strokePath(p, PathStrokeType(1.0f));
		}

		void drawButtonText(Graphics& g, TextButton& button, bool, bool) override
		{
			g.setColour(button.getToggleState() ? Colour(0xAA111111) : button.findColour(TextButton::ColourIds::buttonColourId).withAlpha(1.0f));
			g.setFont(GLOBAL_FONT());
			g.drawText(button.getButtonText(), button.getLocalBounds().toFloat(), Justification::centred);
		}



		void drawAutocompleteItem(Graphics& g, TextEditorWithAutocompleteComponent& parent, const String& itemName, Rectangle<float> itemBounds, bool selected) override
		{
			if (itemName.isEmpty())
				return;

			auto p = dynamic_cast<CreateNodePopup*>(&parent);

			NodeDatabase db;

			if (selected)
			{
				g.setColour(Colours::white.withAlpha(0.05f));
				g.fillRoundedRectangle(itemBounds.translated(-5.0f, 0.0f), 4.0f);
			}

			String nodeId = p->currentFactory;

			auto hasFactory = nodeId.isNotEmpty();

			nodeId << itemName;

			auto description = p->descriptions[nodeId];

			auto bf = GLOBAL_BOLD_FONT();
			auto f = GLOBAL_FONT();

			g.setFont(GLOBAL_BOLD_FONT());

			if (hasFactory)
			{
				auto fIds = db.getFactoryIds(true);
				auto idx = fIds.indexOf(p->currentFactory.upToFirstOccurrenceOf(".", false, false));

				g.setColour(Helpers::getFadeColour(idx, fIds.size()).withAlpha(1.0f));

				g.fillRoundedRectangle(itemBounds.removeFromLeft(4.0f).withSizeKeepingCentre(4.0f, 4.0f), 2.0f);
				itemBounds.removeFromLeft(5.0f);

				g.setColour(Colours::white.withAlpha(0.8f));
				g.drawText(itemName, itemBounds.removeFromLeft(bf.getStringWidthFloat(itemName) + 2.0f), Justification::left);
				g.setColour(Colours::white.withAlpha(0.5f));
			}
			else
			{
				auto factory = itemName.upToFirstOccurrenceOf(".", false, false);
				auto nodeId = itemName.fromFirstOccurrenceOf(".", true, false);

				auto fIds = db.getFactoryIds(true);
				auto idx = fIds.indexOf(factory);


				g.setColour(Helpers::getFadeColour(idx, fIds.size()).withAlpha(1.0f));
				g.drawText(factory, itemBounds.removeFromLeft(bf.getStringWidthFloat(factory) + 2.0f), Justification::left);
				g.setColour(Colours::white.withAlpha(0.8f));
				g.drawText(nodeId, itemBounds.removeFromLeft(bf.getStringWidthFloat(nodeId) + 2.0f), Justification::left);
				g.setColour(Colours::white.withAlpha(0.5f));
			}

			itemBounds.removeFromLeft(5.0f);

			g.setFont(GLOBAL_FONT());
			g.drawText(description, itemBounds, Justification::left);
		}

	} laf;

	std::map<String, String> descriptions;

	CreateNodePopup(DspNetworkComponent& parent, Point<int> pos, const BuildHelpers::CreateData& cd_) :
		TextEditorWithAutocompleteComponent(),
		cd(cd_),
		root(&parent),
		originalPosition(pos),
		zp(parent.findParentComponentOfClass<ZoomableViewport>()),
		descriptions(parent.db.getDescriptions())
	{
		setName("Create new node");

		setLookAndFeel(&laf);

		zp->addZoomListener(this);

		addFactoryButton("All");

		for (auto f : parent.db.getFactoryIds(!cd.isCableNode))
		{
			addFactoryButton(f);
		}

		factoryButtons.getFirst()->setConnectedEdges(Button::ConnectedOnLeft);
		factoryButtons.getLast()->setConnectedEdges(Button::ConnectedOnRight);

		for (int i = 1; i < factoryButtons.size() - 1; i++)
		{
			factoryButtons[i]->setConnectedEdges(Button::ConnectedOnRight | Button::ConnectedOnLeft);
			factoryButtons[i]->setColour(TextButton::ColourIds::buttonColourId, Helpers::getFadeColour(i, factoryButtons.size()));
		}

		factoryButtons.getLast()->setColour(TextButton::ColourIds::buttonColourId, Helpers::getFadeColour(factoryButtons.size() - 1, factoryButtons.size()));

		itemsToShow = 7;
		useDynamicAutocomplete = true;

		addAndMakeVisible(editor);
		initEditor();

		updateTextEditorOnItemChange = true;

		int h = 0;

		h += 10;
		h += 24;
		h += 32;
		h += 5;
		h += 24;
		h += 10;

		int w = 10;

		for (auto tb : factoryButtons)
			w += tb->getWidth();

		w += 10;

		setSize(jmax(w, 500), h);
		GlobalHiseLookAndFeel::setTextEditorColours(editor);
		editor.setColour(TextEditor::ColourIds::highlightedTextColourId, Colours::black);
		editor.setColour(CaretComponent::ColourIds::caretColourId, Colours::black);
		editor.grabKeyboardFocusAsync();
		editor.setTextToShowWhenEmpty("Type to create node", Colours::grey);

		factoryButtons[0]->setToggleState(true, dontSendNotification);

		zp->setSuspendWASD(true);
		//zp->scrollToRectangle(getBoundsInParent(), true);

		updatePosition();

#if 0
		tags.add(new Tag(PropertyIds::IsProcessingHiseEvent, "MIDI"));
		tags.add(new Tag(PropertyIds::IsPolyphonic, "Polyphonic"));
		tags.add(new Tag(PropertyIds::IsControlNode, "Modulation Output"));
		tags.add(new Tag(PropertyIds::UseUnnormalisedModulation, "Unscaled Modulation"));
#endif
	}

	Point<int> originalPosition;

	void updatePosition()
	{
	}

	void addFactoryButton(const String& factoryId)
	{
		auto nb = new TextButton(factoryId);
		nb->setLookAndFeel(&laf);
		addAndMakeVisible(nb);
		factoryButtons.add(nb);

		nb->setClickingTogglesState(true);
		nb->setRadioGroupId(9012421);
		nb->onClick = [this, nb]()
		{
			auto t = nb->getButtonText();

			if (t == "All")
				t = {};
			else
				t << ".";

			setFactory(t, nb->findColour(TextButton::ColourIds::buttonColourId));
		};

		auto w = GLOBAL_FONT().getStringWidth(factoryId) + 20;

		nb->setSize(w, 24);
	}

	Colour currentFactoryColour;
	String currentFactory;
	Rectangle<float> factoryLabel;

	void setFactory(const String& factory, Colour c)
	{
		if (factory != currentFactory)
		{
			currentFactory = factory;
			currentFactoryColour = c;
			resized();
			repaint();
			dismissAutocomplete();
			editor.setText("", dontSendNotification);
			showAutocomplete({});
			editor.setText(autocompleteItems[0], dontSendNotification);
			editor.grabKeyboardFocusAsync();
		}
	}

	OwnedArray<TextButton> factoryButtons;

	ZoomableViewport* zp;

	void zoomChanged(float) override {}

	void positionChanged(Point<int>) override
	{
		updatePosition();
	}

	~CreateNodePopup()
	{
		if (root != nullptr)
		{
			root->currentBuildPath = {};
			root->repaint();
		}

		if (zp != nullptr)
		{
			zp->removeZoomListener(this);
			zp->setSuspendWASD(false);
		}

	}

	void textEditorReturnKeyPressed(TextEditor& te) override
	{
		timerCallback();

		TextEditorWithAutocompleteComponent::textEditorReturnKeyPressed(te);
		auto text = editor.getText();

		if (currentFactory.isNotEmpty())
			text = currentFactory + text;


		Array<ValueTree> newNodes;

		auto v = BuildHelpers::createNode(root->db, text, cd, &root->um);

		if (v.isValid())
		{
			newNodes.add(v);

			auto r = root;

			MessageManager::callAsync([newNodes, r]()
			{
				r->setSelection(newNodes);
				r->grabKeyboardFocusAsync();
			});

			dismiss();
		}
	}

	void dismiss()
	{
		findParentComponentOfClass<NetworkParent>()->dismissPopup();
	}

	void textEditorEscapeKeyPressed(TextEditor& te) override
	{
		TextEditorWithAutocompleteComponent::textEditorEscapeKeyPressed(te);
		dismiss();
	}

	void textEditorTextChanged(TextEditor& te) override
	{
		TextEditorWithAutocompleteComponent::textEditorTextChanged(te);

		if (autocompleteItems.contains(te.getText()))
			textEditorReturnKeyPressed(te);
	}

	TextEditor* getTextEditor() override { return &editor; }

	Identifier getIdForAutocomplete() const override
	{
		if (currentFactory.isEmpty())
			return {};

		return Identifier(currentFactory);
	}

	void autoCompleteItemSelected(int selectedIndex, const String& item) override
	{
		TextEditorWithAutocompleteComponent::autoCompleteItemSelected(selectedIndex, item);

		auto fullPath = currentFactory + item;

		for (auto t : tags)
			t->update(fullPath);

		repaint();
	}

	void resized() override
	{
		auto b = getLocalBounds().reduced(10);

		auto w = b.getWidth() / factoryButtons.size();

		auto top = b.removeFromTop(24);

		for (auto fb : factoryButtons)
		{
			fb->setTopLeftPosition(top.getTopLeft());
			top.removeFromLeft((fb->getWidth()));
		}

		descriptionBounds = b.removeFromTop(32).toFloat();

		if (currentFactory.isNotEmpty())
		{
			auto intend = GLOBAL_BOLD_FONT().getStringWidth(currentFactory) + 10;
			factoryLabel = b.removeFromLeft(intend).toFloat();
		}

		editor.setBounds(b);
	}

	void paint(Graphics& g) override
	{
		g.fillAll(Colour(0xFF262626));

		auto b = getLocalBounds().toFloat();

		if (currentFactory.isNotEmpty())
		{
			g.setColour(currentFactoryColour);
			g.setFont(GLOBAL_BOLD_FONT());
			g.drawText(currentFactory, factoryLabel, Justification::left);
		}

		auto copy = descriptionBounds;

		for (auto t : tags)
			t->draw(g, copy);
	}

	struct Tag : public PathFactory
	{
		Tag(const Identifier& id_, const String& prettyName_) :
			id(id_),
			prettyName(prettyName_),
			p(createPath(prettyName))
		{};

		void draw(Graphics& g, Rectangle<float>& bounds)
		{
			g.setColour(Colours::white.withAlpha(value ? 0.8f : 0.2f));

			scalePath(p, bounds.removeFromLeft(bounds.getHeight()).reduced(6.0f));

			g.fillPath(p);

			auto f = GLOBAL_FONT();
			auto w = f.getStringWidth(prettyName) + 10;
			auto tb = bounds.removeFromLeft(w);

			g.setFont(f);
			g.drawText(prettyName, tb, Justification::centred);
		}

		Path createPath(const String& url) const override
		{
			Path p;

			LOAD_EPATH_IF_URL("MIDI", HiBinaryData::SpecialSymbols::midiData);
			LOAD_EPATH_IF_URL("Polyphonic", HiBinaryData::ProcessorEditorHeaderIcons::polyphonicPath);
			LOAD_EPATH_IF_URL("Modulation Output", ColumnIcons::targetIcon);

			if (url == "Unscaled Modulation")
			{
				static const unsigned char pathData[] = { 110,109,123,204,59,68,236,201,154,68,108,123,204,59,68,174,135,158,68,98,123,204,59,68,184,198,158,68,70,102,59,68,154,249,158,68,98,232,58,68,154,249,158,68,108,92,199,51,68,154,249,158,68,98,121,73,51,68,154,249,158,68,68,227,50,68,184,198,158,68,68,
		227,50,68,174,135,158,68,108,68,227,50,68,225,130,154,68,98,68,227,50,68,41,68,154,68,121,73,51,68,246,16,154,68,92,199,51,68,246,16,154,68,108,98,232,58,68,246,16,154,68,98,70,102,59,68,246,16,154,68,123,204,59,68,41,68,154,68,123,204,59,68,225,130,
		154,68,108,123,204,59,68,0,200,154,68,108,35,99,58,68,0,200,154,68,108,35,99,58,68,51,163,154,68,108,254,252,56,68,51,163,154,68,108,254,252,56,68,92,255,156,68,98,254,252,56,68,174,79,157,68,231,219,56,68,225,138,157,68,219,153,56,68,0,176,157,68,98,
		190,87,56,68,31,213,157,68,47,237,55,68,174,231,157,68,12,90,55,68,174,231,157,68,98,57,196,54,68,174,231,157,68,131,88,54,68,31,213,157,68,233,22,54,68,82,176,157,68,98,63,213,53,68,51,139,157,68,106,180,53,68,164,80,157,68,106,180,53,68,0,0,157,68,
		108,106,180,53,68,51,163,154,68,108,238,76,52,68,51,163,154,68,108,238,76,52,68,10,15,157,68,98,238,76,52,68,236,129,157,68,152,142,52,68,51,219,157,68,219,17,53,68,61,26,158,68,98,31,149,53,68,154,89,158,68,162,85,54,68,72,121,158,68,100,83,55,68,72,
		121,158,68,98,12,82,56,68,72,121,158,68,90,20,57,68,154,89,158,68,61,154,57,68,61,26,158,68,98,49,32,58,68,51,219,157,68,35,99,58,68,143,130,157,68,35,99,58,68,82,16,157,68,108,35,99,58,68,236,201,154,68,108,123,204,59,68,236,201,154,68,99,101,0,0 };

				p.loadPathFromData(pathData, sizeof(pathData));
				return p;
			}

			return p;
		}

		void update(const String& currentPath)
		{
			NodeDatabase db;

			if (auto obj = db.getProperties(currentPath))
			{
				value = (bool)obj->getProperty(id);
			}
		}

		Identifier id;
		String prettyName;
		bool value;
		Path p;
	};

	OwnedArray<Tag> tags;

	TextEditor editor;

	Rectangle<float> descriptionBounds;

	BuildHelpers::CreateData cd;

	Component::SafePointer<DspNetworkComponent> root;
};

void DspNetworkComponent::showCreateConnectionPopup(Point<int> pos)
{
	auto p = NetworkParent::getNetworkParent(this);
	auto lp = dynamic_cast<Component*>(p)->getLocalPoint(this, pos);
	Rectangle<int> a(lp, lp);

	if(auto c = rootComponent->getInnerContainer(this, pos, nullptr))
	{
		showPopup(new CreateConnectionPopup(*this, c->getValueTree()), rootComponent, a.withSizeKeepingCentre(10, 10));
	}

	
}








void DspNetworkComponent::showCreateNodePopup(Point<int> position, Component* target, const Path& buildPath, const BuildHelpers::CreateData& cd)
{
	selection.deselectAll();

	auto p = NetworkParent::getNetworkParent(this);
	
	p->createSignalNodes = !cd.isCableNode;

	currentBuildPath = buildPath;

	auto nc = new CreateNodePopup(*this, position, cd);

	auto bb = p->asComponent()->getLocalArea(this, buildPath.getBounds().toNearestInt().removeFromRight(10));

	showPopup(nc, target, bb.translated(20, 0));
	nc->showAutocomplete({});

	repaint();
}

void DspNetworkComponent::DraggedNode::addToParent(DspNetworkComponent& root)
{
	if (originalParent != nullptr && node != nullptr)
	{
		node->setAlpha(1.0f);

		root.removeChildComponent(node);
		originalParent->addChildComponent(node);
		originalParent->cables.updatePins(*originalParent);
		node->setBounds(originalBounds);
	}
}

void DspNetworkComponent::DraggedNode::removeFromParent(DspNetworkComponent& root)
{
	if (originalParent != nullptr && node != nullptr)
	{
		originalBounds = node->getBoundsInParent();

		root.currentlyDraggedComponents.add(*this);
		originalParent->removeChildComponent(node);
		root.addChildComponent(node);

		originalParent->cables.updatePins(*originalParent);
		node->setAlpha(0.5f);
		auto rootBounds = root.getLocalArea(originalParent, originalBounds);
		node->setBounds(rootBounds);
	}
}

DspNetworkComponent::SnapShot::SnapShot(const ValueTree& rootTree, Rectangle<int> viewport_) :
	viewport(viewport_)
{
	valuetree::Helpers::forEach(rootTree, [&](const ValueTree& n)
		{
			if (n.getType() == PropertyIds::Node)
			{
				Item ni;
				ni.v = n;
				ni.pos = Helpers::getBounds(n, false);
				ni.folded = n[PropertyIds::Folded];
				items.push_back(ni);
			}

			return false;
		});
}

void DspNetworkComponent::SnapShot::restore(ZoomableViewport& zp, UndoManager* um)
{
	for (auto& i : items)
	{
		i.v.setProperty(PropertyIds::Folded, i.folded, um);
		Helpers::updateBounds(i.v, i.pos, um);
	}

	ZoomableViewport* z = &zp;
	auto pos = viewport;

	MessageManager::callAsync([z, pos]()
		{
			z->zoomToRectangle(pos.expanded(20));
		});
}

}