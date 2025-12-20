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



void NodeComponent::HeaderComponent::mouseDoubleClick(const MouseEvent& event)
{
	if(Helpers::isRootNode(parent.getValueTree()))
		return;

	if(event.mods.isRightButtonDown())
		return;

	if (parent.getValueTree()[PropertyIds::Locked])
	{
		DspNetworkComponent::switchRootNode(this, parent.getValueTree());
	}
	else
	{
		parent.toggle(PropertyIds::Folded);

		Helpers::fixOverlap(parent.getValueTree(), parent.um, false);

		auto c = parent.findParentComponentOfClass<ContainerComponent>();

		MessageManager::callAsync([c]()
		{
			c->cables.updatePins(*c);
		});
	}
}

void NodeComponent::HeaderComponent::mouseDown(const MouseEvent& e)
{
	CHECK_MIDDLE_MOUSE_DOWN(e);

	if(Helpers::isRootNode(parent.getValueTree()))
		return;

	if(e.mods.isRightButtonDown() && parent.getValueTree()[PropertyIds::Folded])
	{
		if(auto pc = parent.createPopupComponent())
		{
			DspNetworkComponent::showPopup(pc, &parent);
		}

		return;
	}

	downPos = e.getPosition();
	parent.toFront(false);

	auto ls = findParentComponentOfClass<SelectableComponent::Lasso>();

	auto& selection = ls->getLassoSelection();

	if (!selection.isSelected(&parent))
		selection.addToSelectionBasedOnModifiers(&parent, e.mods);

	for (auto s : ls->getLassoSelection().getItemArray())
	{
		if (s != nullptr && s->isNode())
			selectionPositions[s] = dynamic_cast<Component*>(s.get())->getBoundsInParent();
	}

	shiftDown = e.mods.isShiftDown();

	dragger.startDraggingComponent(&parent, e);

	originalParent = parent.getParentComponent();
	originalBounds = parent.getBoundsInParent();
}

void NodeComponent::HeaderComponent::mouseDrag(const MouseEvent& e)
{
	CHECK_MIDDLE_MOUSE_DRAG(e);

	if (Helpers::isRootNode(parent.getValueTree()))
		return;

	dragger.dragComponent(&parent, e, this);

	auto thisBounds = parent.getBoundsInParent();

	auto deltaX = thisBounds.getX() - originalBounds.getX();
	auto deltaY = thisBounds.getY() - originalBounds.getY();

	auto ls = findParentComponentOfClass<LassoSource<NodeComponent*>>();

	Rectangle<int> selectionBounds = parent.getBoundsInParent();

	for(auto s: selectionPositions)
	{
		auto nb = s.second.translated(deltaX, deltaY);
		Helpers::updateBounds(s.first->getValueTree(), nb, parent.um);
		selectionBounds = selectionBounds.getUnion(nb);
	}

	Helpers::updateBounds(parent.getValueTree(), parent.getBoundsInParent(), parent.um);

	ContainerComponent::expandParentsRecursive(parent, selectionBounds, true);

	parent.findParentComponentOfClass<Component>()->repaint();

	auto root = findParentComponentOfClass<DspNetworkComponent>();

	if (DspNetworkComponent::isEditModeEnabled(e))
	{
		if (!swapDrag)
		{
			swapDrag = true;
			
			root->clearDraggedComponents();

			for(auto s: root->getLassoSelection().getItemArray())
			{
				if(auto nc = dynamic_cast<NodeComponent*>(s.get()))
				{
					root->setIsDragged(nc);
				}
			}

			callRecursive<ContainerComponent>(root, [](ContainerComponent* c)
			{
				c->repaint();
				return false;
			});
		}
		else
		{
			if (root->currentlyHoveredContainer != nullptr)
			{
				root->currentlyHoveredContainer->cables.setDragPosition({}, {});
			}

			auto rootContainer = dynamic_cast<ContainerComponent*>(root->rootComponent.get());

			auto rootPos = rootContainer->getLocalPoint(this, downPos);

			root->currentlyHoveredContainer = rootContainer->getInnerContainer(root, rootPos, &parent);

			if (parent.hasSignal && root->currentlyHoveredContainer != nullptr)
			{
				auto c = root->currentlyHoveredContainer.getComponent();
				auto dragPos = c->getLocalBounds().getIntersection(c->getLocalArea(root, parent.getBoundsInParent()));
				
				c->cables.setDragPosition(dragPos, c->getLocalPoint(this, downPos));
			}

			if (root->currentlyHoveredContainer != nullptr)
				root->currentlyHoveredContainer->repaint();
		}
	}
	else
	{
		if(swapDrag)
		{
			root->currentlyHoveredContainer = nullptr;
			swapDrag = false;
			callRecursive<ContainerComponent>(root, [](ContainerComponent* c){ c->cables.setDragPosition({}, {}); return false; });
		}
	}
}

void NodeComponent::HeaderComponent::mouseUp(const MouseEvent& e)
{
	downPos = {};

	CHECK_MIDDLE_MOUSE_UP(e);

	if (Helpers::isRootNode(parent.getValueTree()))
		return;

	if(e.mods.isRightButtonDown())
		return;

	auto root = findParentComponentOfClass<DspNetworkComponent>();

	selectionPositions.clear();

	if (swapDrag)
	{
		swapDrag = false;

		root->resetDraggedBounds();

		if (root->currentlyHoveredContainer != nullptr)
		{
			auto dropIndex = root->currentlyHoveredContainer->cables.dropIndex;

			if (dropIndex != -1 || !parent.hasSignal)
			{
				//auto newBounds = root->currentlyHoveredContainer->cables.dragPosition;
				//Helpers::updateBounds(parent.getValueTree(), newBounds, parent.um);
				
				auto newParent = root->currentlyHoveredContainer->getValueTree().getChildWithName(PropertyIds::Nodes);
				auto um = parent.um;

				root->currentlyHoveredContainer->repaint();
				root->repaint();

				root->currentlyHoveredContainer->cables.dragPosition = {};
				root->currentlyHoveredContainer = nullptr;

				root->clearDraggedComponents();

				Array<ValueTree> nodesToMove;

				for(auto l: root->getLassoSelection().getItemArray())
				{
					if(l != nullptr)
					{
						auto vt = l->getValueTree();
						if (vt.getType() == PropertyIds::Node)
							nodesToMove.add(l->getValueTree());
					}
				}
				
				BuildHelpers::cleanBeforeMove(nodesToMove, newParent, um);

				for(const auto& n: nodesToMove)
				{
					n.getParent().removeChild(n, um);
					newParent.addChild(n, dropIndex++, um);
				}

				Helpers::fixOverlap(Helpers::getCurrentRoot(parent.getValueTree()), um, false);

				callRecursive<ContainerComponent>(root, [](ContainerComponent* c){ c->cables.setDragPosition({}, {}); return false; });
				root->rebuildCables();
				return;
			}
		}

		root->clearDraggedComponents();
		Helpers::updateBounds(parent.getValueTree(), parent.getBoundsInParent(), parent.um);
		Helpers::fixOverlap(Helpers::getCurrentRoot(parent.getValueTree()), &root->um, false);
		
	}
	else
	{
		auto bounds = parent.getBoundsInParent();
		Helpers::updateBounds(parent.getValueTree(), bounds, parent.um);

		auto currentRoot = Helpers::getCurrentRoot(parent.getValueTree());

		Helpers::fixOverlap(Helpers::getCurrentRoot(parent.getValueTree()), &root->um, false);
	}

	callRecursive<ContainerComponent>(root, [](ContainerComponent* c){ c->cables.setDragPosition({}, {}); return false; });
	root->rebuildCables();
}


Result SelectableComponent::Lasso::create(const Array<ValueTree>& list, Point<int> startPoint)
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

	for (auto n : newTrees)
		container.getChildWithName(PropertyIds::Nodes).addChild(n, insertIndex++, &um);

	Helpers::updateChannelCount(rootTree, false, &um);
	Helpers::fixOverlap(Helpers::getCurrentRoot(container), &um, false);

	MessageManager::callAsync([newTrees, this]()
	{
		setSelection(newTrees);

		auto asC = dynamic_cast<Component*>(this);

		auto zp = asC->findParentComponentOfClass<ZoomableViewport>();

		RectangleList<int> all;

		for (auto& s : selection.getItemArray())
		{
			auto sc = dynamic_cast<Component*>(s.get());

			if (sc != nullptr)
				all.addWithoutMerging(asC->getLocalArea(sc, sc->getLocalBounds()));
		}

		NetworkParent::scrollToRectangle(asC, all.getBounds(), true);
	});

	return Result::ok();
}

void SelectableComponent::Lasso::groupSelection()
{
	if (selection.getNumSelected() == 0)
		return;

	NodeComponent* firstComponent = nullptr;

	for(auto s: selection.getItemArray())
	{
		if(auto nc = dynamic_cast<NodeComponent*>(s.get()))
		{
			firstComponent = nc;
			break;
		}
	}

	if(firstComponent == nullptr)
		return;

	auto container = firstComponent->findParentComponentOfClass<ContainerComponent>();

	auto groupTree = container->getValueTree().getOrCreateChildWithName(UIPropertyIds::Groups, &um);

	StringArray sa;

	for (auto n : selection.getItemArray())
	{
		if (n != nullptr)
		{
			auto id = n->getValueTree()[PropertyIds::ID].toString();

			if (id.isNotEmpty())
				sa.add(id);
				
		}
	}

	sa.sort(false);

	auto s = sa.joinIntoString(";");

	auto existingGroup = groupTree.getChildWithProperty(PropertyIds::Value, s);

	if (existingGroup.isValid())
		groupTree.removeChild(existingGroup, &um);
	else
	{
		ValueTree ng(UIPropertyIds::Group);
		ng.setProperty(PropertyIds::ID, "GROUP", nullptr);
		ng.setProperty(PropertyIds::Value, s, nullptr);
		groupTree.addChild(ng, -1, &um);
	}
}



void NodeComponent::HeaderComponent::BreadcrumbButton::mouseUp(const MouseEvent& e)
{
	NetworkParent::setNewContent(this, data);
}

void NodeComponent::onFold(const Identifier& id, const var& newValue)
{
	auto folded = (bool)newValue;

	parameters.clear();
	modOutputs.clear();

	

	if (folded)
	{
		Array<ValueTree> automatedParameters;

		for (auto p : getValueTree().getChildWithName(PropertyIds::Parameters))
		{
			if (p[PropertyIds::Automated])
				automatedParameters.add(p);
		}

		Array<ValueTree> connectedOutputs;

		valuetree::Helpers::forEach(getValueTree(), [&](const ValueTree& v)
			{
				if (v.getType() == PropertyIds::Connection)
				{
					connectedOutputs.addIfNotAlreadyThere(ParameterHelpers::findConnectionParent(v));
				}

				return false;
			});

		for (auto i : automatedParameters)
			addAndMakeVisible(parameters.add(new FoldedInput(getUpdater(), i, um)));

		for (auto o : connectedOutputs)
			addAndMakeVisible(modOutputs.add(new FoldedOutput(o, um)));

		setFixSize({});
	}
	else
	{
		rebuildDefaultParametersAndOutputs();
		auto boundsToUse = Helpers::getBounds(getValueTree(), false);
		setBounds(boundsToUse);
	}
}

}