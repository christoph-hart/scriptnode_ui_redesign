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






struct ContainerComponent : public NodeComponent,
							public ContainerComponentBase
{
	struct CableSetup
	{
		enum class ContainerType
		{
			Serial,
			Split,
			Multi,
			ModChain,
			Branch,
			Offline
		};

		struct Connection
		{
			int sourceIndex;
			int targetIndex;
			bool active;
		};

		struct Pins
		{
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
		int activeBranch = -1;

		CableSetup(ContainerComponent& parent_, const ValueTree& v) :
			parent(parent_)
		{
			pin = Helpers::createPinHole();
			init(v);

			if(type == ContainerType::Branch)
				branchListener.setCallback(v.getChildWithName(PropertyIds::Parameters).getChild(0), { PropertyIds::Value }, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onBranch));
		}

		valuetree::PropertyListener branchListener;

		bool isSerialType() const {
			return type == ContainerType::Serial || type == ContainerType::ModChain;

		}

		void onBranch(const Identifier&, const var& newValue)
		{
			activeBranch = (int)newValue;

			connections.clear();

			for (int i = 0; i < numNodes; i++)
			{
				MultiChannelConnection mc;

				for (int c = 0; c < numChannels; c++)
					mc.push_back({ c, c, i == activeBranch });

				connections.push_back(mc);
			}

			updatePins(parent);
		}

		void updatePins(ContainerComponent& p);

		void init(const ValueTree& v)
		{
			connections.clear();

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

				auto numPerNode = numNodes == 0 ? numChannels : numChannels / numNodes;

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
			}
			else if (n == "modchain")
			{
				type = ContainerType::ModChain;
				numChannels = 1;

				MultiChannelConnection mc;

				mc.push_back({ 0, 0, true });

				connections.push_back(mc);
			}
			else if (n == "offline")
			{
				type = ContainerType::Offline;
				numChannels = 0;
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

		struct CableDrawData
		{
			Point<float> p1;
			Point<float> p2;
			Colour c;
			int channelIndex;
			int numChannels;
			float cableOffset;
			int lod;
			bool active = true;

			void setCoordinates(Point<float> p1_, Point<float> p2_, float cableOffset_, int channelIndex_)
			{
				p1 = p1_;
				p2 = p2_;
				cableOffset = cableOffset_;
				channelIndex = channelIndex_;
			}

			void draw(Graphics& g, bool preferLines)
			{
				Path p;

				if (preferLines)
				{
					p.startNewSubPath(p1);
					p.lineTo(p2);
				}
				else
				{
					float offset = 0.0f;

					auto sShape = p2.getX() < p1.getX();

					if (numChannels > 1)
					{
						if (sShape)
						{
							auto fullOffset = -6.0f;

							auto sOffset = (1.0f - (float)channelIndex / (float)numChannels) * fullOffset;

							p1 = p1.translated(sOffset, 0.0f);
							p2 = p2.translated(-sOffset, 0.0f);
						}
						else
						{
							auto maxWidth = jmin((float)Helpers::SignalHeight * 0.25f, 0.8f * std::abs((p2.getX() - p1.getX())));

							float normalisedChannelIndex = (float)channelIndex / (float)(numChannels - 1);
							offset = maxWidth * (-0.5f + normalisedChannelIndex);

							if (p2.getY() > p1.getY())
								offset *= -1.0f;
						}
					}

					offset += cableOffset;

					p.startNewSubPath(p1);

					Helpers::createCustomizableCurve(p, p1, p2, offset, 5.0f);
				}

				
				PathStrokeType::EndCapStyle sp = PathStrokeType::EndCapStyle::butt;

				auto strokeDepth = LayoutTools::getCableThickness(lod);

				if(lod == 0 && active)
				{

					g.setColour(Colours::black.withAlpha(0.7f));
					g.fillEllipse(Rectangle<float>(p1, p1).withSizeKeepingCentre(4.0f, 4.0f));
					g.fillEllipse(Rectangle<float>(p2, p2).withSizeKeepingCentre(4.0f, 4.0f));
					g.strokePath(p, PathStrokeType(3.0f));
					sp = PathStrokeType::rounded;
				}
				
				g.setColour(active ? c : c.withAlpha(0.2f));
				g.strokePath(p, PathStrokeType(strokeDepth, PathStrokeType::beveled, sp));
			}
		};

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

			for(auto ab: parent.addButtons)
			{
				auto lp = ab->getLocalPoint(&parent, downPos).toFloat();
				dropIndex = jmax(dropIndex, ab->onNodeDrag(lp));
			}

			parent.repaint();
		}

		float getCableOffset(int nodeIndex, Point<float> p1, Point<float> p2) const
		{
			return getCableOffset(nodeIndex, std::abs(p2.getX() - p1.getX()));
		}

		float getCableOffset(int nodeIndex, float maxWidth) const
		{
			ValueTree n;

			if(nodeIndex == 0)
				n = parent.getValueTree().getChildWithName(PropertyIds::Nodes);
			else
				n = parent.getValueTree().getChildWithName(PropertyIds::Nodes).getChild(nodeIndex - 1);

			if(n.isValid())
			{
				auto v = (float)n[UIPropertyIds::CableOffset];
				return jlimit(-maxWidth * 0.4f, maxWidth * 0.4f, v);
			}

			return 0.0f;
		}

		int lod = 0;

		void draw(Graphics& g)
		{
			auto colour = Helpers::getNodeColour(parent.getValueTree());

			if(!isSerialType() && pinPositions.childPositions.size() != connections.size())
				return;

			const auto delta = (float)(Helpers::SignalHeight) / (float)numChannels;

			CableDrawData cd;
			cd.lod = LODManager::getLOD(parent);
			cd.numChannels = numChannels;
			cd.c = colour;

			pinPositions.containerStart.setX(cd.lod == 0 ? Helpers::ParameterMargin + Helpers::ParameterWidth - 25.0f : 0);

			auto xOffset = 10.0f;

			for (int c = 0; c < numChannels; c++)
			{
				Rectangle<float> s(pinPositions.containerStart, pinPositions.containerStart);
				Rectangle<float> e(pinPositions.containerEnd, pinPositions.containerEnd);

				auto pinSize = 8.0f;
				s = s.withSizeKeepingCentre(pinSize, pinSize).translated(xOffset, c * delta + delta * 0.5f);
				e = e.withSizeKeepingCentre(pinSize, pinSize).translated(-xOffset, c * delta + delta * 0.5f);

				if(cd.lod == 0)
				{
					g.setColour(Colours::grey);
					PathFactory::scalePath(pin, s);
					g.fillPath(pin);

					if (type != ContainerType::ModChain)
					{
						PathFactory::scalePath(pin, e);
						g.fillPath(pin);
					}
				}
				
				

				if (!routeThroughChildNodes())
				{
					cd.setCoordinates(s.getCentre(), e.getCentre(), 0.0f, c);
					cd.draw(g, true);

					//drawSignalCable(g, s.getCentre(), e.getCentre(), colour, true, c, numChannels, 0.0f);
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
						auto o = getCableOffset(0, p1, p2);

						cd.setCoordinates(p1.translated(0, offset), p2.translated(0, offset), o, c);
						cd.draw(g, false);
						//drawSignalCable(g, p1.translated(0, offset), p2.translated(0, offset), colour, false, c, numChannels, o);
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
							auto o = getCableOffset(i+1, p1, p2);

							cd.setCoordinates(p1.translated(offset, 0), p2.translated(offset, 0), o, c);
							cd.draw(g, true);
							//drawSignalCable(g, p1.translated(offset, 0), p2.translated(offset, 0), colour, true, c, numChannels, o);
							offset += delta;
						}
					}
					else
					{
						float offset = delta * 0.5f;

						for (int c = 0; c < numChannels; c++)
						{
							auto o = getCableOffset(i+1, p1, p2);
							cd.setCoordinates(p1.translated(0, offset), p2.translated(0, offset), o, c);
							cd.draw(g, false);
							//drawSignalCable(g, p1.translated(0, offset), p2.translated(0, offset), colour, false, c, numChannels, o);
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

						auto o = getCableOffset(pinPositions.childPositions.size(), p1, p2);
						cd.setCoordinates(p1.translated(0, offset), p2.translated(-xOffset, offset), o, c);
						cd.draw(g, false);
						//drawSignalCable(g, p1.translated(0, offset), p2.translated(-xOffset, offset), colour, false, c, numChannels, o);
						offset += delta;
					}
				}
			}
			else
			{
				int cIndex = 0;

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

						cd.channelIndex = cIndex;
						cd.cableOffset = 0.0f;
						cd.active = cc.active;

						cd.p1 = p1;
						cd.p2 = p2;

						cd.draw(g, false);

						cd.p1 = p3;
						cd.p2 = p4;

						cd.draw(g, false);

						//drawSignalCable(g, p1, p2, colour, false, cIndex, numChannels, 0.0f);
						//drawSignalCable(g, p3, p4, colour, false, cIndex, numChannels, 0.0f);

						cIndex++;
					}
				}
			}

		}

		ContainerComponent& parent;
		bool isForcedVertical = false;
		ContainerType type;
		int numChannels = 0;
		int numNodes = 0;
		std::vector<MultiChannelConnection> connections;
		Rectangle<int> dragPosition;
		int dropIndex = -1;

		Pins pinPositions;
	};

	ContainerComponent(Lasso* l, const ValueTree& v, UndoManager* um_) :
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

		lockButton.setToggleStateAndUpdateIcon((bool)v[UIPropertyIds::LockPosition]);

		lockButton.onClick = [this]()
		{
			toggle(UIPropertyIds::LockPosition);
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

		lockListener.setCallback(data.getChildWithName(PropertyIds::Nodes), { PropertyIds::Locked }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onChildLock));

		resizer.setAlwaysOnTop(true);

		childPositionListener.setCallback(v.getChildWithName(PropertyIds::Nodes), 
			UIPropertyIds::Helpers::getPositionIds(), 
			Helpers::UIMode, 
			VT_BIND_RECURSIVE_PROPERTY_LISTENER(onChildPositionUpdate));

		auto commentTree = getValueTree().getOrCreateChildWithName(UIPropertyIds::Comments, um);

		freeCommentListener.setCallback(commentTree, Helpers::UIMode, VT_BIND_CHILD_LISTENER(onFreeComment));

		channelListener.setCallback(v, { PropertyIds::CompileChannelAmount }, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onChannel));

		auto gt = getValueTree().getOrCreateChildWithName(UIPropertyIds::Groups, um);
		groupListener.setCallback(gt, Helpers::UIMode, VT_BIND_CHILD_LISTENER(onGroup));

		nodeListener.handleUpdateNowIfNeeded();
		resizeListener.handleUpdateNowIfNeeded();
		groupListener.handleUpdateNowIfNeeded();

		if(!Helpers::hasDefinedBounds(v))
		{
			Rectangle<int> nb(getPosition(), getPosition().translated(500, 100));

			Helpers::updateBounds(getValueTree(), nb, um);
			setSize(500, 100);
			resized();
			cables.updatePins(*this);
		}

		parameterDragger.onOffset({}, data[UIPropertyIds::ParameterYOffset]);

		bypassListener.setCallback(getValueTree(), { PropertyIds::Bypassed }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onBypass));

		commentListener.setCallback(getValueTree(), { PropertyIds::Comment }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onCommentChange));

		rebuildDescription();
	}

	struct Group
	{
		Group(ContainerComponent& parent_, const ValueTree& v_):
		  name(v_[PropertyIds::ID].toString()),
		  v(v_),
		  parent(parent_)
		{
			auto list = v[PropertyIds::Value].toString();
			idList = StringArray::fromTokens(list, ";", "");
			idList.removeDuplicates(false);
			idList.removeEmptyStrings();
			idList.trim();
		}

		bool operator==(const ValueTree& other) const { return v == other; }

		void draw(Graphics& g);

		Rectangle<float> lastBounds;
		ValueTree v;
		ContainerComponent& parent;
		String name;
		StringArray idList;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Group);
	};

	OwnedArray<Group> groups;

	void onGroup(const ValueTree& v, bool wasAdded)
	{
		if(wasAdded)
		{
			groups.add(new Group(*this, v));
		}
		else
		{
			for(auto g: groups)
			{
				if(*g == v)
				{
					groups.removeObject(g);
					break;
				}
			}
		}

		repaint();
	}

	void onFreeComment(const ValueTree& v, bool wasAdded)
	{
		if(wasAdded)
		{
			comments.add(new Comment(*this, v));
		}
		else
		{
			for(auto c: comments)
			{
				if(c->data == v)
				{
					comments.removeObject(c);
					break;
				}
			}
		}
	}

	void onChannel(const Identifier&, const var& newValue)
	{
		cables.init(getValueTree());
		cables.updatePins(*this);
	}

	void onChildLock(const ValueTree& v, const Identifier& id)
	{
		if(Helpers::isImmediateChildNode(v, getValueTree()))
		{
			onChildAddRemove(v, false);
			onChildAddRemove(v, true);
		}
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
						comments.getLast()->showEditor();

						return;
					}
				}
			}
			else
			{
				if(v.getType() == PropertyIds::Comment)
				{
					v.getParent().removeChild(v, um);
					return;
				}

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

		auto folded = (bool)newValue && !Helpers::isRootNode(getValueTree());

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
				// only update cables when the nodes is not being dragged around...
				for(auto cn: childNodes)
				{
					if(cn->getValueTree() == v)
					{
						if(cn->getParentComponent() == this)
						{
							SafeAsyncCall::call<ContainerComponent>(*this, [](ContainerComponent& c)
							{
								c.cables.updatePins(c);
							});
						}

						break;
					}
				}
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

	void renameGroup(Point<float> position)
	{
		for (auto g : groups)
		{
			if (g->lastBounds.contains(position))
			{
				auto copy = g->lastBounds;

				addAndMakeVisible(groupEditor = new TextEditor());
				groupEditor->setBounds(copy.removeFromTop(20).toNearestInt());

				GlobalHiseLookAndFeel::setTextEditorColours(*groupEditor);
				groupEditor->getTextValue().referTo(g->v.getPropertyAsValue(PropertyIds::ID, um, false));
				groupEditor->grabKeyboardFocusAsync();

				groupEditor->onFocusLost = [this]()
				{
					groupEditor = nullptr;
				};
			}
		}
	}

	ScopedPointer<TextEditor> groupEditor;

	bool selectGroup(Point<float> position)
	{
		auto ok = false;

		for(auto g: groups)
		{
			if(g->lastBounds.contains(position))
			{
				auto& selection = findParentComponentOfClass<Lasso>()->getLassoSelection();

				for(auto cn: childNodes)
				{
					if(g->idList.contains(cn->getValueTree()[PropertyIds::ID].toString()))
					{
						selection.addToSelection(cn);
						ok = true;
					}
				}
			}
		}

		return ok;
	}

	CablePinBase* createOutsideParameterComponent(const ValueTree& source) override
	{
		auto conParent = ParameterHelpers::findConnectionParent(source);

		if (conParent.getType() == PropertyIds::Parameter)
			return new ParameterComponent(getUpdater(), conParent, asNodeComponent().getUndoManager());
		else
			return new ModulationBridge(conParent, asNodeComponent().getUndoManager());
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

		auto showBottomTools = !Helpers::isFoldedOrLockedContainer(getValueTree());

		lockButton.setVisible(showBottomTools);
		resizer.setVisible(showBottomTools);

		auto b = getLocalBounds();
		auto bar = b.removeFromBottom(15);
		resizer.setBounds(bar.removeFromRight(15));
		cables.updatePins(*this);
		lockButton.setBounds(bar.removeFromRight(15));

		auto yOffset = jmax(Helpers::HeaderHeight + Helpers::SignalHeight, (int)data[UIPropertyIds::ParameterYOffset] + 2 * Helpers::ParameterMargin);

		for(auto p: parameters)
		{
			yOffset += Helpers::ParameterMargin;
			p->setTopLeftPosition(p->getX(), yOffset);
			yOffset += p->getHeight();
		}

		positionOutsideParameters(yOffset);
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
			auto b = getLocalBounds().reduced(20, 3).toFloat();

			float alpha = 0.05f;

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
		Comment(ContainerComponent& parent_, const ValueTree& d):
		  parent(parent_),
		  data(d),
		  renderer("")
		{
			auto sd = renderer.getStyleData();
			sd.fontSize = 14.0f;
			renderer.setStyleData(sd);

			positionListener.setCallback(data,
			{ PropertyIds::Comment,
				UIPropertyIds::CommentWidth,
				UIPropertyIds::CommentOffsetX,
				UIPropertyIds::CommentOffsetY,
			}, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onUpdate));

			parent.addAndMakeVisible(this);
		}

		Comment(ContainerComponent& parent_, NodeComponent* attachedNode_) :
			parent(parent_),
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

			if(data.getType() == PropertyIds::Comment)
			{
				expandParentsRecursive(*this, getBoundsInParent(), false);
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

				if(data.getType() == PropertyIds::Comment)
				{
					setTopLeftPosition(currentOffset);
				}
				else
				{
					setTopLeftPosition(Helpers::getBounds(data, false).getTopRight().translated(10 + currentOffset.getX(), currentOffset.getY()));
				}
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
	};

	OwnedArray<Comment> comments;

	struct AddButton: public Component
	{
		AddButton(ContainerComponent& parent_, const Path& hitzone_, int index_):
		  p(parent.createPath("add")),
		  parent(parent_),
		  hitzone(hitzone_),
		  index(index_)
		{
			setBounds(hitzone.getBounds().toNearestInt());

			hitzone.applyTransform(AffineTransform::translation(getPosition()).inverted());
			parent.addAndMakeVisible(this);
			toBack();
			setRepaintsOnMouseActivity(true);
		}

		~AddButton()
		{
			int x = 5;
		}

		bool hitTest(int x, int y) override
		{
			Point<float> pos((float)x, (float)y);
			return hitzone.contains(pos);
		}

		void mouseDown(const MouseEvent& e) override
		{
			SelectableComponent::Lasso::checkLassoEvent(e, SelectableComponent::LassoState::Down);

			downOffset = getNodeBefore()[UIPropertyIds::CableOffset];
			repaint();
		}

		ValueTree getNodeBefore()
		{
			if(index == 0)
				return parent.getValueTree().getChildWithName(PropertyIds::Nodes);
			else
				return parent.getValueTree().getChildWithName(PropertyIds::Nodes).getChild(index-1);
		}

		void mouseDrag(const MouseEvent& e) override
		{
			SelectableComponent::Lasso::checkLassoEvent(e, SelectableComponent::LassoState::Drag);

			setMouseCursor(MouseCursor::LeftRightResizeCursor);

			auto dx = e.getDistanceFromDragStartX();

			getNodeBefore().setProperty(UIPropertyIds::CableOffset, downOffset + dx, parent.um);
			parent.repaint();
		}

		float downOffset = 0.0f;

		int onNodeDrag(Point<float> lp)
		{
			auto prev = draggedOver;

			draggedOver = contains(lp);

			if(prev && !draggedOver)
			{
				int x = 5;
			}

			repaint();
			return draggedOver ? index : -1;
		}

		bool draggedOver = false;

		void mouseUp(const MouseEvent& e) override;
		
		void paint(Graphics& g) override
		{
			if(draggedOver)
			{
				g.setColour(Colour(SIGNAL_COLOUR).withAlpha(0.3f));
				g.fillPath(hitzone);
				g.setColour(Colour(SIGNAL_COLOUR));
				g.strokePath(hitzone, PathStrokeType(1.0f));
			}

			if(active && downOffset == 0.0f)
			{
				g.setColour(Colours::white.withAlpha(0.1f));
				g.fillPath(hitzone);

				g.setColour(Colours::white.withAlpha(0.4f));
				g.fillPath(p);
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
			auto b = getLocalBounds().toFloat();

			auto a = Rectangle<float>(b.getCentre(), b.getCentre()).withSizeKeepingCentre(12.0f, 12.0f);

			auto offset = (float)getNodeBefore()[UIPropertyIds::CableOffset];

			a = a.translated(offset, 0.0f);

			parent.scalePath(p, a);
		}

		ContainerComponent& parent;
		
		Path p;

		Path hitzone;

		const int index;
	};

	

	static void expandParentsRecursive(Component& componentToShow, Rectangle<int> firstBoundsMightBeUnion, bool addMarginToFirst)
	{
		Component* c = &componentToShow;
		auto pc = componentToShow.findParentComponentOfClass<ContainerComponent>();

		while (pc != nullptr)
		{
			auto parentBounds = pc->getLocalBounds();
			auto boundsInParent = c->getBoundsInParent();

			auto first = c == &componentToShow;

			if (first && !firstBoundsMightBeUnion.isEmpty())
				boundsInParent = firstBoundsMightBeUnion;

			auto w = boundsInParent.getRight();
			auto h = boundsInParent.getBottom();

			if(addMarginToFirst || !first)
			{
				w += Helpers::NodeMargin;
				h += Helpers::NodeMargin;
			}

			w = jmax<int>(parentBounds.getWidth(), w);
			h = jmax<int>(parentBounds.getHeight(), h);

			pc->getValueTree().setProperty(UIPropertyIds::width, w, pc->um);
			pc->getValueTree().setProperty(UIPropertyIds::height, h, pc->um);

			c = pc;
			pc = pc->findParentComponentOfClass<ContainerComponent>();
		}
	}

	

	
	

	OwnedArray<AddButton> addButtons;
	CableSetup cables;

	HiseShapeButton lockButton;

	struct Resizer: public juce::ResizableCornerComponent
	{
		Resizer(Component* c):
			ResizableCornerComponent(c, nullptr)
		{};

		void paint(Graphics& g) override
		{
			Path p;
			p.startNewSubPath(1.0, 0.0);
			p.lineTo(1.0, 1.0);
			p.lineTo(0.0, 1.0);
			p.closeSubPath();

			auto over = isMouseOver(true);

			PathFactory::scalePath(p, getLocalBounds().toFloat().reduced(over ? 2.f : 3.0f));

			auto c = Helpers::getNodeColour(findParentComponentOfClass<ContainerComponent>()->getValueTree());
			g.setColour(c);
			g.fillPath(p);
		}

		void mouseDrag(const MouseEvent& e) override
		{
			ResizableCornerComponent::mouseDrag(e);

			expandParentsRecursive(*this, {}, false);
		}

		void mouseUp(const MouseEvent& e) override;
	} resizer;
	OwnedArray<NodeComponent> childNodes;

	valuetree::PropertyListener resizeListener;
	valuetree::PropertyListener channelListener;
	valuetree::ChildListener nodeListener;

	valuetree::RecursivePropertyListener childPositionListener;

	valuetree::RecursivePropertyListener bypassListener;
	
	valuetree::RecursivePropertyListener commentListener;

	valuetree::RecursivePropertyListener verticalListener;

	valuetree::ChildListener groupListener;
	valuetree::ChildListener freeCommentListener;

	valuetree::RecursivePropertyListener lockListener;

	AttributedString description;

	JUCE_DECLARE_WEAK_REFERENCEABLE(ContainerComponent);
	
};


}