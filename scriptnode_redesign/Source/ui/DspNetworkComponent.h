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

struct DspNetworkComponent : public Component,
							 public NodeComponent::Lasso,
							 public CableComponent::CableHolder,
							 public LODManager
{
	

	enum class Action
	{
		Open,
		DeselectAll,
		Cut,
		Copy,
		Paste,
		Duplicate,
		Delete,
		CreateNew,
		ToggleVertical,
		ToggleEdit,
		ToggleFold,
		Rename,
		AddComment,
		CollapseContainer,
		GroupSelection,
		Undo,
		Redo,
		AutoLayout,
		AlignTop,
		AlignLeft,
		EditXML,
		SetColour,
		HideCable,
		DistributeHorizontally,
		DistributeVertically,
		ShowMap,
		Back,
		Forward,
		numActions
	};

	DspNetworkComponent(PooledUIUpdater* updater, ZoomableViewport& zp, const ValueTree& networkTree, const ValueTree& container);
	
	void onFold(const ValueTree& v, const Identifier& id);
	void onContainerResize(const Identifier& id, const var& newValue);

	static bool isEditModeEnabled(const MouseEvent& e);

	void paint(Graphics& g) override;
	void resized() override;

	void mouseMove(const MouseEvent& e) override;
	void mouseDown(const MouseEvent& e) override;
	void mouseDrag(const MouseEvent& e) override;
	void mouseDoubleClick(const MouseEvent& e) override;
	void mouseUp(const MouseEvent& e) override;

	void toggleEditMode();
	bool performAction(Action a);
	bool keyPressed(const KeyPress& k) override;

	Rectangle<int> getCurrentViewPosition() const;

	void findLassoItemsInArea(Array<SelectableComponent::WeakPtr>& itemsFound,
		const Rectangle<int>& area) override;

	void setIsDragged(NodeComponent* nc);
	void clearDraggedComponents();
	void resetDraggedBounds();

	static void showPopup(Component* popupComponent, Component* target, Rectangle<int> specialBounds = {});
	static void switchRootNode(Component* c, ValueTree newRoot);

	Component::SafePointer<ContainerComponent> currentlyHoveredContainer;
	ScopedPointer<ContainerComponent> rootComponent;

	void showCreateConnectionPopup(Point<int> pos);
	void showCreateNodePopup(Point<int> position, Component* target, const Path& buildPath, const BuildHelpers::CreateData& cd);

	ValueTree getRootTree() const { return rootComponent->getValueTree(); }

private:

	struct CreateConnectionPopup;
	struct CreateNodePopup;

	struct DraggedNode
	{
		void addToParent(DspNetworkComponent& root);

		void removeFromParent(DspNetworkComponent& root);

		Rectangle<int> originalBounds;
		Component::SafePointer<ContainerComponent> originalParent;
		Component::SafePointer<NodeComponent> node;
	};

	
	
	struct SnapShot
	{
		SnapShot() = default;

		SnapShot(const ValueTree& rootTree, Rectangle<int> viewport_);

		void restore(ZoomableViewport& zp, UndoManager* um);

		struct Item
		{
			ValueTree v;
			Rectangle<int> pos;
			bool folded;
		};

		Rectangle<int> viewport;
		std::vector<Item> items;
	};

	bool editMode = false;

	Array<DraggedNode> currentlyDraggedComponents;
	std::map<char, SnapShot> snapshotPositions;

	Path currentBuildPath;

	ValueTree data;

	Point<int> pos;

	valuetree::PropertyListener rootSizeListener;
	valuetree::RecursivePropertyListener foldListener;

	JUCE_DECLARE_WEAK_REFERENCEABLE(DspNetworkComponent);
};



}
