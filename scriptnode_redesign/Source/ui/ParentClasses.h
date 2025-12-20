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

struct NetworkParent : public TextEditorWithAutocompleteComponent::Parent,
					   public ZoomableViewport::ZoomListener,
					   public Timer
{
	virtual ~NetworkParent() = default;

	Component* asComponent() { return dynamic_cast<Component*>(this); }

	static NetworkParent* getNetworkParent(Component* c);

	ZoomableViewport* getViewport();

	void timerCallback() override
	{
		viewUndoManager.beginNewTransaction();
	}

	struct Map : public Component
	{
		struct Item
		{
			void draw(Graphics& g, const AffineTransform& tr);

			Rectangle<float> area;
			String name;
			Colour c;
		};

		Map(const ValueTree& v, NetworkParent& p_, Rectangle<int> viewPosition_);

		~Map()
		{
			int x = 5;
		}

		void mouseDown(const MouseEvent& e) override;
		void mouseDrag(const MouseEvent& e) override;
		void mouseUp(const MouseEvent& e) override
		{
			findParentComponentOfClass<NetworkParent>()->setKeepPopupAlive(false);
		}

		void paint(Graphics& g) override;
		void updatePosition(Rectangle<float> newPos);
		
		Array<Item> items;

		Rectangle<float> viewPosition;
		Rectangle<float> downPosition;
		NetworkParent& p;
		AffineTransform tr, sc; // NORM ->  MAP BOU UNDS
		Rectangle<float> fullBounds;
	};

	struct PopupCodeEditor : public Component,
							 public valuetree::AnyListener
	{
		PopupCodeEditor(const ValueTree& v, const Identifier& id_, UndoManager* um_) :
			d(v),
			id(id_),
			um(um_),
			doc(codeDoc),
			editor(doc),
			resizer(this, nullptr)
		{
			

			if (id == PropertyIds::Comment)
				editor.setLanguageManager(new mcl::MarkdownLanguageManager());

			String content;

			if(id.isValid())
				content = v[id].toString();
			else
			{
				content = v.createXml()->createDocument("");
				editor.setLanguageManager(new mcl::XmlLanguageManager());
				setRootValueTree(d);
				setMillisecondsBetweenUpdate(100);
			}
			
			codeDoc.replaceAllContent(content);
			codeDoc.clearUndoHistory();

			addAndMakeVisible(editor);
			addAndMakeVisible(resizer);

			setSize(400, 300);
		}

		~PopupCodeEditor()
		{
			if(id.isValid())
				updateValue();
		}

		void anythingChanged(CallbackType cb) override
		{
			auto newContent = d.createXml()->createDocument("");
			codeDoc.replaceAllContent(newContent);
			codeDoc.clearUndoHistory();
		}

		void updateValue()
		{
			if(id.isValid())
			{
				d.setProperty(id, codeDoc.getAllContent(), um);
			}
			else
			{
				if(auto xml = XmlDocument::parse(codeDoc.getAllContent()))
				{
					auto p = d.getParent();
					auto idx = p.indexOf(d);
					p.removeChild(d, um);
					auto nv = ValueTree::fromXml(*xml);
					p.addChild(nv, idx, um);
					d = nv;
				}
			}
		}

		static void show(Component* attachedComponent, const ValueTree& data, const Identifier& id, UndoManager* um)
		{
			auto root = NetworkParent::getNetworkParent(attachedComponent);
			auto editor = new PopupCodeEditor(data, id, um);
			root->showPopup(editor, attachedComponent, {});
		}

		bool keyPressed(const KeyPress& key) override
		{
			if(key == KeyPress::F5Key)
			{
				updateValue();
				return true;
			}

			return false;
		}

		void resized() override
		{
			editor.setBounds(getLocalBounds());
			resizer.setBounds(getLocalBounds().removeFromBottom(15).removeFromRight(15));
		}

		ValueTree d;
		const Identifier id;
		UndoManager* um;

		juce::CodeDocument codeDoc;
		mcl::TextDocument doc;
		mcl::TextEditor editor;

		juce::ResizableCornerComponent resizer;
	};

	struct PopupWrapper : public Component,
						  public ComponentMovementWatcher,
						  public Timer
	{
		PopupWrapper(Component* c);

		void timerCallback() override;
		bool keyPressed(const KeyPress& k) override;
		void paint(Graphics& g) override;
		void resized() override;

		void mouseDown(const MouseEvent& e) override
		{
			dragger.startDraggingComponent(this, e);
		}

		void mouseDrag(const MouseEvent& e) override
		{
			dragger.dragComponent(this, e, nullptr);
		}

		void componentPeerChanged() override {};
		void componentVisibilityChanged() override {}

		void componentMovedOrResized(bool wasMoved, bool wasResized) override
		{
			auto b = getBoundsInParent();
			auto cb = content->getLocalBounds();
			setBounds(b.withSizeKeepingCentre(cb.getWidth() + 20, cb.getHeight() + 50));
		}

		ComponentDragger dragger;
		ScopedPointer<Component> content;
	};

	NetworkParent()
	{
		startTimer(500);
	}

	void showMap(const ValueTree& v, Rectangle<int> viewPosition, Point<int> position);

	void positionChanged(Point<int> newPosition) override;
	void zoomChanged(float newZoomFactor) override;
	void dismissPopup();

	void showPopup(Component* popupComponent, Component* target, Rectangle<int> specialBounds/* = */);

	StringArray getAutocompleteItems(const Identifier& id) override;

	bool createSignalNodes = false;
	NodeDatabase db;
	ScopedPointer<PopupWrapper> currentPopup;

	void setKeepPopupAlive(bool shouldKeepAlive)
	{
		if(keepAlive != shouldKeepAlive)
		{
			keepAlive = shouldKeepAlive;

			if(!keepAlive)
				dismissPopup();
		}
	}

	bool keepAlive = false;

	struct NavigationAction: public UndoableAction
	{
		NavigationAction(NetworkParent& p_, Rectangle<int> newRectangle, bool animate_);

		bool perform() override
		{
			p.getViewport()->scrollToRectangle(newArea, true, animate);
			return true;
		}

		bool undo() override
		{
			p.getViewport()->scrollToRectangle(oldArea, true, animate);
			return true;
		}

		NetworkParent& p;
		Rectangle<int> oldArea, newArea;
		bool animate;
	};

	struct RootNavigation: public UndoableAction
	{
		RootNavigation(NetworkParent& p_, const ValueTree& v);

		bool perform() override;

		bool undo() override;

		NetworkParent& p;
		ValueTree oldRoot, newRoot;
		Rectangle<int> oldArea;
	};

	static void setNewContent(Component* nc, const ValueTree& newRoot)
	{
		if(auto p = nc->findParentComponentOfClass<NetworkParent>())
		{
			p->getViewUndoManager()->perform(new RootNavigation(*p, newRoot));
		}
	}

	static void scrollToRectangle(Component* c, Rectangle<int> newRectangle, bool animate)
	{
		if (auto p = c->findParentComponentOfClass<NetworkParent>())
		{
			p->getViewUndoManager()->perform(new NavigationAction(*p, newRectangle, animate));
		}
	}

	UndoManager* getViewUndoManager() { return &viewUndoManager; }

	UndoManager viewUndoManager;
};

}