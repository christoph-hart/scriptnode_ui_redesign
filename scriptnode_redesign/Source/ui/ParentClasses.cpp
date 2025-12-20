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



scriptnode::NetworkParent* NetworkParent::getNetworkParent(Component* c)
{
	return c->findParentComponentOfClass<NetworkParent>();
}

hise::ZoomableViewport* NetworkParent::getViewport()
{
	ZoomableViewport* zp = nullptr;

	Component::callRecursive<ZoomableViewport>(asComponent(), [&](ZoomableViewport* zp_)
		{
			zp = zp_;
			return true;
		});

	return zp;
}



void NetworkParent::showMap(const ValueTree& v, Rectangle<int> viewPosition, Point<int> position)
{
	auto m = new Map(v, *this, viewPosition);
	showPopup(m, getViewport(), viewPosition);
	currentPopup->setCentrePosition(position);
	
}

void NetworkParent::positionChanged(Point<int> newPosition)
{
	dismissPopup();
}

void NetworkParent::zoomChanged(float newZoomFactor)
{
	dismissPopup();
}

void NetworkParent::dismissPopup()
{
	if(!keepAlive)
	{
		currentPopup = nullptr;
		getViewport()->removeZoomListener(this);
		getViewport()->getContent<Component>()->grabKeyboardFocusAsync();
	}
}

void NetworkParent::showPopup(Component* popupComponent, Component* target, Rectangle<int> specialBounds/* = */)
{
	getViewport()->addZoomListener(this);

	auto zoomFactor = jmax(1.0f, getViewport()->getCurrentZoomFactor());

	

	

	auto dp = target->findParentComponentOfClass<DspNetworkComponent>();

	auto c = dynamic_cast<Component*>(this);

	if (specialBounds.isEmpty())
		specialBounds = c->getLocalArea(target, target->getLocalBounds());

	currentPopup = new PopupWrapper(popupComponent);
	
	auto fullBounds = c->getLocalBounds();

	auto lb = currentPopup->getLocalBounds();
	lb.setPosition(specialBounds.getCentre());

	currentPopup->setBounds(lb.constrainedWithin(fullBounds));
	c->addAndMakeVisible(currentPopup);
	currentPopup->grabKeyboardFocus();

	auto tr = AffineTransform::scale(zoomFactor, zoomFactor, currentPopup->getX(), currentPopup->getY());
	currentPopup->setTransform(tr);
}

juce::StringArray NetworkParent::getAutocompleteItems(const Identifier& id)
{
	auto list = db.getNodeIds(createSignalNodes);

	if (id.isValid())
	{
		auto prefix = id.toString();

		StringArray matches;
		matches.ensureStorageAllocated(list.size());

		for (const auto& l : list)
		{
			if (l.startsWith(prefix))
				matches.add(l.fromFirstOccurrenceOf(prefix, false, false));
		}

		return matches;
	}

	return list;
}

NetworkParent::PopupWrapper::PopupWrapper(Component* c) :
	ComponentMovementWatcher(c),
	content(c),
	dragger()
{
	addAndMakeVisible(content);
	setSize(c->getWidth() + 20, c->getHeight() + 50);
	setWantsKeyboardFocus(true);
	startTimer(200);
	c->addComponentListener(this);
	setMouseCursor(MouseCursor::DraggingHandCursor);
}

void NetworkParent::PopupWrapper::timerCallback()
{
	auto fc = getCurrentlyFocusedComponent();

	if (TextEditorWithAutocompleteComponent::isAutocomplete(fc))
		return;

	while (fc != nullptr)
	{
		if (dynamic_cast<ParameterComponent*>(fc) != nullptr)
			return;

		if (fc == this)
		{
			return;
		}

		fc = fc->getParentComponent();
	}

	findParentComponentOfClass<NetworkParent>()->dismissPopup();
}

bool NetworkParent::PopupWrapper::keyPressed(const KeyPress& k)
{
	if (k == KeyPress::escapeKey)
	{
		findParentComponentOfClass<NetworkParent>()->dismissPopup();
		return true;
	}

	return false;
}

void NetworkParent::PopupWrapper::paint(Graphics& g)
{
	g.setColour(Colour(0xFF262626));
	g.fillRoundedRectangle(getLocalBounds().reduced(5.0f).toFloat(), 4.0f);

	g.setColour(Colours::white.withAlpha(0.5f));
	g.drawRoundedRectangle(getLocalBounds().reduced(5.0f).toFloat(), 4.0f, 2.0f);
	g.setFont(GLOBAL_BOLD_FONT());
	g.drawText(content->getName(), getLocalBounds().removeFromTop(30).toFloat(), Justification::centred);
}

void NetworkParent::PopupWrapper::resized()
{
	auto b = getLocalBounds().reduced(10);
	b.removeFromTop(30);
	content->setBounds(b);
}

void NetworkParent::Map::Item::draw(Graphics& g, const AffineTransform& tr)
{
	auto rb = area.transformed(tr);

	g.setColour(c);

	g.drawRect(rb, 1);

	auto tb = rb.removeFromTop(13.0f);

	g.setColour(c.withAlpha(0.1f));
	g.fillRect(tb);
	g.setFont(GLOBAL_FONT().withHeight(10.0f));
	g.setColour(Colours::white);
	g.drawText(name, tb, Justification::centred);
}

NetworkParent::Map::Map(const ValueTree& v, NetworkParent& p_, Rectangle<int> viewPosition_) :
	p(p_)
{
	setMouseCursor(MouseCursor::DraggingHandCursor);

	fullBounds = Helpers::getBounds(v, false).toFloat();
	fullBounds.setWidth((int)v[UIPropertyIds::width]);
	fullBounds.setHeight((int)v[UIPropertyIds::height]);
	sc = AffineTransform::scale(fullBounds.getWidth(), fullBounds.getHeight(), 0.0f, 0.0f).inverted();

	viewPosition = viewPosition_.toFloat().transformed(sc);

	Helpers::forEachVisibleNode(v, [&](const ValueTree& n)
		{
			auto x = Helpers::getBoundsInRoot(n, false).toFloat();

			if (n == v)
				x = fullBounds;

			x = x.translated(-fullBounds.getX(), -fullBounds.getY());
			x = x.transformed(sc);

			Item ni;
			ni.area = x;
			ni.name = Helpers::getHeaderTitle(n);
			ni.c = Helpers::getNodeColour(n);

			items.add(ni);
		}, true);

	auto w = 500;
	auto h = w / fullBounds.getWidth() * fullBounds.getHeight();
	setSize(w, h);

	tr = AffineTransform::scale((float)w, (float)h, 0.0f, 0.0f);
}

void NetworkParent::Map::mouseDown(const MouseEvent& e)
{
	findParentComponentOfClass<NetworkParent>()->setKeepPopupAlive(true);

	auto pos = e.position.transformedBy(tr.inverted());

	if (!viewPosition.contains(pos))
	{
		auto np = viewPosition;
		np.setCentre(pos.getX(), pos.getY());
		updatePosition(np);
	}

	downPosition = viewPosition;
	repaint();
}

void NetworkParent::Map::mouseDrag(const MouseEvent& e)
{
	Point<float> delta(e.getDistanceFromDragStartX(), e.getDistanceFromDragStartY());

	delta = delta.transformedBy(tr.inverted());

	updatePosition(downPosition.translated(delta.getX(), delta.getY()));
}

void NetworkParent::Map::paint(Graphics& g)
{
	g.fillAll(Colour(0xEE333333));

	auto b = getLocalBounds().toFloat();

	g.setColour(Colours::white.withAlpha(0.2f));
	g.fillRect(viewPosition.transformed(tr));

	for (auto i : items)
	{
		i.draw(g, tr);
	}
}

void NetworkParent::Map::updatePosition(Rectangle<float> newPos)
{
	newPos = newPos.constrainedWithin(Rectangle<float>(0.0f, 0.0f, 1.0f, 1.0f));

	if (newPos != viewPosition)
	{
		viewPosition = newPos;

		auto newArea = viewPosition.transformedBy(sc.inverted()).toNearestInt();

		scrollToRectangle(this, newArea, false);

		repaint();
	}
}

NetworkParent::NavigationAction::NavigationAction(NetworkParent& p_, Rectangle<int> newRectangle, bool animate_) :
	UndoableAction(),
	p(p_),
	newArea(newRectangle),
	animate(animate_)
{
	oldArea = p.getViewport()->getContent<DspNetworkComponent>()->getCurrentViewPosition();
}

NetworkParent::RootNavigation::RootNavigation(NetworkParent& p_, const ValueTree& v) :
	UndoableAction(),
	p(p_),
	newRoot(v)
{
	auto dp = p.getViewport()->getContent<DspNetworkComponent>();

	oldRoot = dp->getRootTree();
	oldArea = dp->getCurrentViewPosition();
}

bool NetworkParent::RootNavigation::perform()
{
	auto root = valuetree::Helpers::getRoot(newRoot);

	if(!root.isValid())
		return false;

	auto dp = p.getViewport()->getContent<DspNetworkComponent>();
	auto updater = dp->getUpdater();

	auto zp = p.getViewport();
	zp->setNewContent(new DspNetworkComponent(updater, *zp, root, newRoot), nullptr);

	return true;
}

bool NetworkParent::RootNavigation::undo()
{
	auto root = valuetree::Helpers::getRoot(oldRoot);

	if(!root.isValid())
		return false;

	auto dp = p.getViewport()->getContent<DspNetworkComponent>();
	auto updater = dp->getUpdater();

	auto zp = p.getViewport();
	zp->setNewContent(new DspNetworkComponent(updater, *zp, root, oldRoot), nullptr);
	

	auto a = oldArea;

	Timer::callAfterDelay(510, [zp, a]()
	{
		zp->zoomToRectangle(a);
	});

	return true;
}

}