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




struct ContainerComponent : public NodeComponent
{
	struct CableSetup
	{
		enum class ContainerType
		{
			Serial,
			Split,
			Multi,
			ModChain,
			Branch
		};

		struct Connection
		{
			int sourceIndex;
			int targetIndex;
			bool active;
		};

		struct Pins
		{
			std::vector<std::pair<Rectangle<int>, int>> makeHitZones(Rectangle<int> fullBounds, bool vertical) const
			{
				std::vector<std::pair<Rectangle<int>, int>> hz;

				if (childPositions.empty())
				{
					hz.push_back({ Rectangle<float>(containerStart, containerEnd).toNearestInt(), 0 });
				}
				else
				{
					hz.push_back({ Rectangle<float>(containerStart, childPositions[0].first).toNearestInt(), 0 });

					for (int i = 0; i < childPositions.size() - 1; i++)
					{
						hz.push_back({ Rectangle<float>(childPositions[i].second,
							childPositions[i + 1].first).toNearestInt(), childPositions[i].dropIndex + 1});
					}

					hz.push_back({ Rectangle<float>(childPositions.back().second, containerEnd).toNearestInt(), childPositions.back().dropIndex +1 });
				}

				for (auto& h : hz)
				{
					h.first.setWidth(jmax(h.first.getWidth(), Helpers::SignalHeight));
					h.first.setHeight(jmax(h.first.getHeight(), Helpers::SignalHeight));
					h.first = h.first.expanded(0, Helpers::SignalHeight).constrainedWithin(fullBounds);
				}

				return hz;
			}

			Point<float> containerStart;
			Point<float> containerEnd;

			struct Position
			{
				Point<float> first;
				Point<float> second;
				int dropIndex;
			};


			std::vector<Position> childPositions;
		};

		using MultiChannelConnection = std::vector<Connection>;

		Path pin;

		CableSetup(ContainerComponent& parent_, const ValueTree& v) :
			parent(parent_)
		{
			pin = Helpers::createPinHole();
			init(v);

		}

		bool isSerialType() const {
			return type == ContainerType::Serial || type == ContainerType::ModChain;

		}

		void updatePins(ContainerComponent& p);

		void init(const ValueTree& v)
		{
			connections.clear();

			colour = Helpers::getNodeColour(v);
			numNodes = v.getChildWithName(PropertyIds::Nodes).getNumChildren();
			numChannels = Helpers::getNumChannels(v);

			jassert(Helpers::isContainerNode(v));

			auto n = Helpers::getFactoryPath(v).second;

			if (n == "split")
			{
				type = ContainerType::Split;

				MultiChannelConnection mc;

				for (int i = 0; i < numChannels; i++)
					mc.push_back({ i, i, true });

				for (int i = 0; i < numNodes; i++)
					connections.push_back(mc);
			}
			else if (n == "multi")
			{
				type = ContainerType::Multi;

				auto numPerNode = numChannels / numNodes;

				for (int i = 0; i < numNodes; i++)
				{
					MultiChannelConnection mc;

					for(int c = 0; c < numPerNode; c++)
						mc.push_back({ i * numPerNode + c, c, true });

					connections.push_back(mc);
				}
			}
			else if (n == "branch")
			{
				type = ContainerType::Branch;

				for (int i = 0; i < numNodes; i++)
				{
					MultiChannelConnection mc;

					for (int c = 0; c < numChannels; c++)
						mc.push_back({ c, c, i == 1 });

					connections.push_back(mc);
				}
			}
			else if (n == "modchain")
			{
				type = ContainerType::ModChain;

				MultiChannelConnection mc;

				mc.push_back({ 0, 0, true });

				connections.push_back(mc);
			}
			else
			{
				type = ContainerType::Serial;

				MultiChannelConnection mc;

				for (int i = 0; i < numChannels; i++)
					mc.push_back({ i, i, true });

				connections.push_back(mc);
			}

			updateVerticalState();
		}

		void updateVerticalState()
		{
			isForcedVertical = type == CableSetup::ContainerType::Serial && Helpers::isVerticalContainer(parent.getValueTree());
		}

		static void drawVerticalCable(Graphics& g, Point<float> sourcePoint, Point<float> targetPoint, Colour c)
		{
			drawSignalCable(g, sourcePoint, targetPoint, c);
			return;
#if 0
			Path p;

			p.startNewSubPath(sourcePoint);

			auto controlPointOffset = (targetPoint.getX() - sourcePoint.getX()) * 0.5f; // 50% of the horizontal distance

			juce::Point<float> cp1(sourcePoint.getX() + controlPointOffset, sourcePoint.getY());
			juce::Point<float> cp2(targetPoint.getX() - controlPointOffset, targetPoint.getY());

			p.cubicTo(cp1, cp2, targetPoint);

			Path dashed;
			float d[2] = { 4.0f, 4.0f };

			PathStrokeType(2.0f).createDashedStroke(dashed, p, d, 2);
			std::swap(p, dashed);

			g.fillPath(p);
#endif
		}

		static void drawSignalCable(Graphics& g, Point<float> p1, Point<float> p2, Colour c)
		{
			Path p;
			p.startNewSubPath(p1);

			g.setColour(Colours::black.withAlpha(0.7f));

			g.fillEllipse(Rectangle<float>(p1, p1).withSizeKeepingCentre(4.0f, 4.0f));
			g.fillEllipse(Rectangle<float>(p2, p2).withSizeKeepingCentre(4.0f, 4.0f));

			Helpers::createCurve(p, p1, p2, {}, false);

			//p.lineTo(p2);

			g.strokePath(p, PathStrokeType(3.0f));
			g.setColour(c);

			g.strokePath(p, PathStrokeType(1.0f, PathStrokeType::beveled, PathStrokeType::rounded));
		}

		static void drawParameterCable(Graphics& g, Point<float> p1, Point<float> p2)
		{
			Path p;

			p.startNewSubPath(p1);
			Helpers::createCurve(p, p1, p2, { 0.0f, 0.0f }, true);
			g.strokePath(p, PathStrokeType(1.0f));
		}

		bool isVertical() const
		{
			if (type == ContainerType::Branch || type == ContainerType::Multi || type == ContainerType::Split)
				return true;

			return isForcedVertical;
		}

		bool routeThroughChildNodes() const
		{
			return !pinPositions.childPositions.empty();
		}

		void setDragPosition(Rectangle<int> currentlyDraggedNodeBounds, Point<int> downPos)
		{
			dragPosition = currentlyDraggedNodeBounds;

			dropIndex = -1;
			dragRuler = {};

			auto hitzones = pinPositions.makeHitZones(parent.getLocalBounds(), isVertical());

			if (!dragPosition.isEmpty())
			{
				for (const auto& hz : hitzones)
				{
					if(hz.first.contains(downPos))
					//if (!dragPosition.getIntersection(hz.first).isEmpty())
					{
						dropIndex = hz.second;

						Rectangle<float> rb;

						if (isVertical())
						{
							rb = { 3.0f, (float)hz.first.getCentreY(), (float)parent.getWidth() - 6.0f, 3.0f };
						}
						else
						{
							rb = { (float)hz.first.getCentreX(),
								   (float)Helpers::HeaderHeight + 5.0f,
								   3.0f,
								   (float)parent.getHeight() - 6.0f - (float)Helpers::HeaderHeight };
						}

						dragRuler = rb;
						break;
					}
				}
			}

			parent.repaint();

		}

		void draw(Graphics& g)
		{
			if(!isSerialType() && pinPositions.childPositions.size() != connections.size())
				return;

			if(!dragRuler.isEmpty())
			{
				g.setColour(Colours::white.withAlpha(0.2f));
				g.fillRoundedRectangle(dragRuler, 2.5f);
			}
			
			const auto delta = (float)(Helpers::SignalHeight) / (float)numChannels;

			auto xOffset = 10.0f;

			for (int c = 0; c < numChannels; c++)
			{
				Rectangle<float> s(pinPositions.containerStart, pinPositions.containerStart);
				Rectangle<float> e(pinPositions.containerEnd, pinPositions.containerEnd);

				auto pinSize = 8.0f;
				s = s.withSizeKeepingCentre(pinSize, pinSize).translated(xOffset, c * delta + delta * 0.5f);
				e = e.withSizeKeepingCentre(pinSize, pinSize).translated(-xOffset, c * delta + delta * 0.5f);

				g.setColour(Colours::grey);
				PathFactory::scalePath(pin, s);
				g.fillPath(pin);

				if(type != ContainerType::ModChain)
				{
					PathFactory::scalePath(pin, e);
					g.fillPath(pin);
				}

				if (!routeThroughChildNodes())
				{
					drawSignalCable(g, s.getCentre(), e.getCentre(), colour);
				}
			}

			if (!routeThroughChildNodes())
				return;

			g.setColour(colour);

			if (isSerialType())
			{
				if (pinPositions.childPositions.size() > 0)
				{
					// draw first

					auto firstPos = pinPositions.childPositions[0];
					float offset = delta * 0.5f;

					for (int c = 0; c < numChannels; c++)
					{
						auto p1 = pinPositions.containerStart.translated(xOffset, 0.0f);
						auto p2 = firstPos.first;
						drawSignalCable(g, p1.translated(0, offset), p2.translated(0, offset), colour);
						offset += delta;
					}
				}

				for (int i = 0; i < pinPositions.childPositions.size() - 1; i++)
				{
					auto p1 = pinPositions.childPositions[i].second;
					auto p2 = pinPositions.childPositions[i + 1].first;

					if (isForcedVertical)
					{
						float offset = (float)Helpers::SignalHeight * -0.25f;

						for (int c = 0; c < numChannels; c++)
						{
							drawVerticalCable(g, p1.translated(offset, 0), p2.translated(offset, 0), colour);
							offset += delta;
						}
					}
					else
					{
						float offset = delta * 0.5f;

						for (int c = 0; c < numChannels; c++)
						{
							drawSignalCable(g, p1.translated(0, offset), p2.translated(0, offset), colour);
							offset += delta;
						}
					}
				}

				if (type != ContainerType::ModChain && pinPositions.childPositions.size() > 0)
				{
					// draw last
					auto lastPos = pinPositions.childPositions.back();
					float offset = delta * 0.5f;

					for (int c = 0; c < numChannels; c++)
					{
						auto p1 = lastPos.second;
						auto p2 = pinPositions.containerEnd;
						drawSignalCable(g, p1.translated(0, offset), p2.translated(-xOffset, offset), colour);
						offset += delta;
					}
				}
			}
			else
			{
				for (int i = 0; i < connections.size(); i++)
				{
					std::vector<Connection> con = connections[i];

					for (auto cc : con)
					{
						auto numTargets = con.size();
						auto targetDelta = Helpers::SignalHeight / numTargets;

						float offset = delta * 0.5f + (float)cc.sourceIndex * delta;
						float targetOffset = targetDelta * 0.5f + (float)cc.targetIndex * targetDelta;
						auto p1 = pinPositions.containerStart.translated(xOffset, offset);
						auto p2 = pinPositions.childPositions[i].first.translated(0, targetOffset);
						auto p3 = pinPositions.childPositions[i].second.translated(0, targetOffset);
						auto p4 = pinPositions.containerEnd.translated(-xOffset, offset);
						drawSignalCable(g, p1, p2, colour);
						drawSignalCable(g, p3, p4, colour);
					}
				}
			}

		}

		Rectangle<float> dragRuler;

		ContainerComponent& parent;
		bool isForcedVertical = false;
		ContainerType type;
		Colour colour;
		int numChannels = 0;
		int numNodes = 0;
		std::vector<MultiChannelConnection> connections;
		Rectangle<int> dragPosition;
		int dropIndex = -1;

		Pins pinPositions;
	};

	ContainerComponent(Lasso& l, const ValueTree& v, UndoManager* um_) :
		NodeComponent(l, v, um_, true),
		cables(*this, v),
		resizer(this),
		lockButton("lock", nullptr, *this),
		parameterDragger(*this)
	{
		

		addAndMakeVisible(resizer);
		addAndMakeVisible(lockButton);

		lockButton.setVisible(!Helpers::isRootNode(v));

		lockButton.setToggleModeWithColourChange(true);

		lockButton.setToggleStateAndUpdateIcon((bool)v[UIPropertyIds::Locked]);

		lockButton.onClick = [this]()
		{
			toggle(UIPropertyIds::Locked);
		};

		setInterceptsMouseClicks(false, true);
		nodeListener.setCallback(data.getChildWithName(PropertyIds::Nodes),
			Helpers::UIMode,
			VT_BIND_CHILD_LISTENER(onChildAddRemove));

		resizeListener.setCallback(data,
			{ UIPropertyIds::width, UIPropertyIds::height, PropertyIds::IsVertical },
			Helpers::UIMode,
			VT_BIND_PROPERTY_LISTENER(onResize));

		verticalListener.setCallback(data,
			{ PropertyIds::Value }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onIsVertical));

		resizer.setAlwaysOnTop(true);

		childPositionListener.setCallback(v.getChildWithName(PropertyIds::Nodes), 
			UIPropertyIds::Helpers::getPositionIds(), 
			Helpers::UIMode, 
			VT_BIND_RECURSIVE_PROPERTY_LISTENER(onChildPositionUpdate));

		nodeListener.handleUpdateNowIfNeeded();
		resizeListener.handleUpdateNowIfNeeded();

		if(!Helpers::hasDefinedBounds(v))
			setFixSize({});

		parameterDragger.onOffset({}, data[UIPropertyIds::ParameterYOffset]);

		bypassListener.setCallback(getValueTree(), { PropertyIds::Bypassed }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onBypass));

		commentListener.setCallback(getValueTree(), { PropertyIds::Comment }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onCommentChange));

		rebuildDescription();
	}

	void rebuildDescription()
	{
		description.clear();
		auto desc = Helpers::getSignalDescription(getValueTree());
		description.setJustification(Justification::centredRight);
		description.append(desc, GLOBAL_FONT(), Helpers::getNodeColour(getValueTree()));
	}

	void onCommentChange(const ValueTree& v, const Identifier& id)
	{
		if(Helpers::isImmediateChildNode(v, getValueTree()))
		{
			auto t = v[id].toString();

			auto add = t.isNotEmpty();

			if(add)
			{
				for(auto& c: comments)
				{
					if(c->data == v)
						return;
				}

				for(auto cn: childNodes)
				{
					if(cn->getValueTree() == v)
					{
						comments.add(new Comment(*this, cn));
						Helpers::fixOverlap(getValueTree(), um, false);

						Component::SafePointer<Comment> c = comments.getLast();
						MessageManager::callAsync([c]()
						{
							if(c.getComponent() != nullptr)
								c->showEditor();
						});
						
						return;
					}
				}
			}
			else
			{
				for(auto c: comments)
				{
					if(c->data == v)
					{
						comments.removeObject(c);
						Helpers::fixOverlap(getValueTree(), um, false);
						return;
					}
				}
			}
		}
	}

	void onBypass(const ValueTree& v, const Identifier&)
	{
		if(Helpers::isImmediateChildNode(v, getValueTree()))
		{
			cables.updatePins(*this);
		}
		if(v == getValueTree())
		{
			rebuildDescription();
			repaint();
		}
	}

	void paint(Graphics& g) override;

	Image cableImage;

	void onFold(const Identifier& id, const var& newValue) override
	{
		NodeComponent::onFold(id, newValue);

		auto folded = (bool)newValue;

		for(auto c: childNodes)
			c->setVisible(!folded);

		cables.updatePins(*this);
	}

	void onChildPositionUpdate(const ValueTree& v, const Identifier& id)
	{
		if(v.getParent() == getValueTree().getChildWithName(PropertyIds::Nodes))
		{
			if(Helpers::isProcessNode(v) || Helpers::isContainerNode(v))
			{
				SafeAsyncCall::call<ContainerComponent>(*this, [](ContainerComponent& c)
				{
					c.cables.updatePins(c);
				});
			}
		}
	}

	void onChildAddRemove(const ValueTree& v, bool wasAdded);

	void onResize(const Identifier& id, const var& newValue)
	{
		if (id == UIPropertyIds::width)
			setSize((int)newValue, getHeight());
		else if (id == UIPropertyIds::height)
			setSize(getWidth(), (int)newValue);
	}

	void onIsVertical(const ValueTree& v, const Identifier& id)
	{
		if(v[PropertyIds::ID] == PropertyIds::IsVertical.toString())
		{
			Helpers::resetLayout(getValueTree(), um);

			MessageManager::callAsync([this]()
			{
				cables.updateVerticalState();
				cables.updatePins(*this);
			});
		}
	}

	ContainerComponent* getInnerContainer(Component* root, Point<int> rootPosition, Component* toSkip)
	{
		if(this == toSkip)
			return nullptr;

		if (root->getLocalArea(this, getLocalBounds()).contains(rootPosition))
		{
			for (auto c : childNodes)
			{
				if (auto childContainer = dynamic_cast<ContainerComponent*>(c))
				{
					auto inner = childContainer->getInnerContainer(root, rootPosition, toSkip);

					if (inner != nullptr)
						return inner;
				}
			}

			return this;
		}

		return nullptr;
	}

	void resized() override
	{
		NodeComponent::resized();

		auto showBottomTools = !(bool)data[PropertyIds::Folded] && !Helpers::isRootNode(data);

		lockButton.setVisible(showBottomTools);
		resizer.setVisible(showBottomTools);

		auto b = getLocalBounds();
		auto bar = b.removeFromBottom(15);
		resizer.setBounds(bar.removeFromRight(15));
		cables.updatePins(*this);
		lockButton.setBounds(bar.removeFromRight(15));

		auto yOffset = jmax(Helpers::HeaderHeight + Helpers::SignalHeight, (int)data[UIPropertyIds::ParameterYOffset]);

		for(auto p: parameters)
		{
			yOffset += Helpers::ParameterMargin;
			p->setTopLeftPosition(p->getX(), yOffset);
			yOffset += p->getHeight();
		}

		if(!outsideParameters.isEmpty())
		{
			if(parameters.isEmpty())
				yOffset += 10;

			outsideLabel = { 0.0f, (float)yOffset, (float)(Helpers::ParameterMargin + Helpers::ParameterWidth), 20.0f };
			yOffset += 20;
		}

		for(auto op: outsideParameters)
		{
			yOffset += Helpers::ParameterMargin;
			op->setTopLeftPosition(op->getX(), yOffset);
			yOffset += op->getHeight();
			
		}
	}

	struct ParameterVerticalDragger: public Component
	{
		ParameterVerticalDragger(ContainerComponent& c):
		  parent(c)
		{
			c.addAndMakeVisible(this);
			setVisible(c.getValueTree().getChildWithName(PropertyIds::Parameters).getNumChildren() > 0);
			setSize(Helpers::ParameterMargin + Helpers::ParameterWidth, 10);

			offsetListener.setCallback(c.getValueTree(), { UIPropertyIds::ParameterYOffset }, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onOffset));
			setMouseCursor(MouseCursor::UpDownResizeCursor);
			setRepaintsOnMouseActivity(true);
		}

		void paint(Graphics& g) override
		{
			auto b = getLocalBounds().reduced(5, 3).toFloat();

			float alpha = 0.2f;

			if(isMouseOverOrDragging())
				alpha += 0.1f;

			if(isMouseButtonDown())
				alpha += 0.2f;

			g.setColour(Colours::white.withAlpha(alpha));
			g.fillRoundedRectangle(b, b.getHeight() * 0.5f);
		}

		void mouseDown(const MouseEvent& e) override
		{
			downY = e.getEventRelativeTo(getParentComponent()).getPosition().getY();
		}

		void mouseDrag(const MouseEvent& e) override;

		void onOffset(const Identifier& id, const var& newValue);

		ContainerComponent& parent;
		valuetree::PropertyListener offsetListener;
		int downY = 0;

	} parameterDragger;

	struct Comment : public Component
	{
		Comment(ContainerComponent& parent_, NodeComponent* attachedNode_) :
			parent(parent_),
			attachedNode(attachedNode_),
			data(attachedNode_->getValueTree()),
			renderer("")
		{
			auto sd = renderer.getStyleData();
			sd.fontSize = 14.0f;
			renderer.setStyleData(sd);

			positionListener.setCallback(data,
				{ PropertyIds::Comment,
				  UIPropertyIds::x,
				  UIPropertyIds::y,
				  UIPropertyIds::width,
				  UIPropertyIds::height,
				  UIPropertyIds::CommentWidth,
				  UIPropertyIds::CommentOffsetX,
				  UIPropertyIds::CommentOffsetY,
				  PropertyIds::NodeColour,
				}, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onUpdate));

			parent.addAndMakeVisible(this);
		}

		void showEditor();

		bool dragRightEdge = false;

		void mouseDoubleClick(const MouseEvent& e) override
		{
			showEditor();
		}

		void mouseMove(const MouseEvent& e) override
		{
			dragRightEdge = getWidth() -  e.getPosition().getX() < 20;
			setMouseCursor(dragRightEdge ? MouseCursor::LeftEdgeResizeCursor : MouseCursor::DraggingHandCursor);
			repaint();
		}

		Point<int> currentOffset;
		Point<int> downOffset;
		int downWidth = 0;

		void mouseDown(const MouseEvent& e) override
		{
			if(dragRightEdge)
			{
				downWidth = getWidth();
			}
			else
			{
				downOffset = currentOffset;
			}
		}

		void mouseUp(const MouseEvent& e) override
		{
			if(e.mouseWasDraggedSinceMouseDown())
				Helpers::fixOverlap(parent.getValueTree(), parent.um, false);
		}

		void mouseDrag(const MouseEvent& e) override
		{
			if(dragRightEdge)
			{
				auto newWidth = jlimit(128, 800, e.getDistanceFromDragStartX() + downWidth);
				data.setProperty(UIPropertyIds::CommentWidth, newWidth, parent.um);
			}
			else
			{
				auto newOffset = downOffset.translated(e.getDistanceFromDragStartX(), e.getDistanceFromDragStartY());
				data.setProperty(UIPropertyIds::CommentOffsetX, newOffset.getX(), parent.um);
				data.setProperty(UIPropertyIds::CommentOffsetY, newOffset.getY(), parent.um);
			}
		}

		void mouseExit(const MouseEvent& e) override
		{
			dragRightEdge = false;
			repaint();
		}

		void onUpdate(const Identifier& id, const var& newValue)
		{
			if(id == UIPropertyIds::x || id == UIPropertyIds::y || 
			   id == UIPropertyIds::CommentOffsetX || id == UIPropertyIds::CommentOffsetY)
			{
				currentOffset = CommentHelpers::getCommentOffset(data);
				setTopLeftPosition(Helpers::getBounds(data, false).getTopRight().translated(10 + currentOffset.getX(), currentOffset.getY()));
			}
			
			if(id == PropertyIds::NodeColour)
			{
				auto c = Helpers::getNodeColour(data);
				auto sd = renderer.getStyleData();
				sd.headlineColour = c;
				renderer.setStyleData(sd);
			}
			if(id == UIPropertyIds::CommentWidth)
			{
				int widthToUse = CommentHelpers::getCommentWidth(data);
				auto h = renderer.getHeightForWidth(widthToUse);
				setSize(widthToUse + 20, h + 20);
			}
			if (id == PropertyIds::Comment)
			{
				renderer.setNewText(newValue.toString());
				auto widthToUse = CommentHelpers::getCommentWidth(data);
				auto h = renderer.getHeightForWidth(widthToUse);
				setSize(widthToUse + 20, h + 20);
			}
		}

		void paint(Graphics& g) override
		{
			auto c = Helpers::getNodeColour(data);

			if(dragRightEdge)
			{
				g.setColour(c);
				g.fillRect(getLocalBounds().removeFromRight(3));
			}

			g.setColour(c.withAlpha(0.05f));
			g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
			renderer.draw(g, getLocalBounds().toFloat().reduced(10.0f));
		}

		void resized() override
		{

		}

		ContainerComponent& parent;
		ValueTree data;

		valuetree::PropertyListener positionListener;

		MarkdownRenderer renderer;

		Component::SafePointer<NodeComponent> attachedNode;
	};

	OwnedArray<Comment> comments;

	struct AddButton: public Component
	{
		AddButton(ContainerComponent& parent_, int index_, Rectangle<int> hitzone):
		  p(parent.createPath("add")),
		  parent(parent_),
		  index(index_)
		{
			parent.addAndMakeVisible(this);

			auto pBounds = parent.getLocalBounds();
			pBounds.removeFromTop(Helpers::HeaderHeight);

			setBounds(hitzone.constrainedWithin(pBounds.reduced(10)));
			toBack();
			setRepaintsOnMouseActivity(true);
		}

		~AddButton()
		{
			int x = 5;
		}

		void mouseDown(const MouseEvent& e) override
		{
			SelectableComponent::Lasso::checkLassoEvent(e, SelectableComponent::LassoState::Down);
		}

		void mouseDrag(const MouseEvent& e) override
		{
			SelectableComponent::Lasso::checkLassoEvent(e, SelectableComponent::LassoState::Drag);
		}

		void mouseUp(const MouseEvent& e) override;
		
		void paint(Graphics& g) override
		{
			if(active)
			{
				g.setColour(Colours::white.withAlpha(0.1f));
				g.fillPath(p);
				auto b = getLocalBounds().withSizeKeepingCentre(3, getHeight()-5).toFloat();
				g.fillRoundedRectangle(b, b.getWidth() * 0.5f);
			}
		}

		bool active = false;

		void mouseExit(const MouseEvent& e) override
		{
			active = false;
			setMouseCursor(MouseCursor::NormalCursor);
			repaint();
		}

		void mouseMove(const MouseEvent& e) override;

		void resized() override 
		{
			auto b = getLocalBounds();


			parent.scalePath(p, b.removeFromTop(12).toFloat().translated(12.0f, 0.0f));
		}

		ContainerComponent& parent;
		
		Path p;
		const int index;
	};

	CablePinBase* getOutsideParameter(const ValueTree& pTree)
	{
		jassert(pTree.getType() == PropertyIds::Parameter);

		for(auto op: outsideParameters)
		{
			if(op->data == pTree)
				return op;
		}
		
		return nullptr;
	}

	void rebuildOutsideParameters()
	{
		outsideParameters.clear();

		auto pTrees = Helpers::getAutomatedChildParameters(getValueTree());

		

		for(auto p: pTrees)
		{
			
			auto np = new ParameterComponent(p, um);
			outsideParameters.add(np);
			addAndMakeVisible(np);
		}

		if(outsideParameters.size() != 0)
			resized();

		parameterDragger.setVisible(!parameters.isEmpty() || !outsideParameters.isEmpty());
	}

	Rectangle<float> outsideLabel;
	OwnedArray<ParameterComponent> outsideParameters;

	OwnedArray<AddButton> addButtons;
	CableSetup cables;

	HiseShapeButton lockButton;

	struct Resizer: public juce::ResizableCornerComponent
	{
		Resizer(Component* c):
			ResizableCornerComponent(c, nullptr)
		{};

		void mouseUp(const MouseEvent& e) override;
	} resizer;
	OwnedArray<NodeComponent> childNodes;

	valuetree::PropertyListener resizeListener;
	valuetree::ChildListener nodeListener;

	valuetree::RecursivePropertyListener childPositionListener;

	valuetree::RecursivePropertyListener bypassListener;
	
	valuetree::RecursivePropertyListener commentListener;

	valuetree::RecursivePropertyListener verticalListener;

	AttributedString description;

	JUCE_DECLARE_WEAK_REFERENCEABLE(ContainerComponent);
	
};


}