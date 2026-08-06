#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace mpp::app
{
	class EditorCommand
	{
	public:
		virtual ~EditorCommand() = default;

		virtual std::string const& name() const = 0;

		virtual void execute() = 0;

		virtual void undo() = 0;
	};

	class CommandStack
	{
		std::vector<std::unique_ptr<EditorCommand>> mCommands;
		size_t mCursor{ 0 };
		size_t mSaveCursor{ 0 };
		bool mSavePointReachable{ true };
		size_t mLimit{ 256 };

		void trimToLimit();

	public:
		explicit CommandStack(size_t limit = 256);

		void execute(std::unique_ptr<EditorCommand> command);

		bool canUndo() const;

		bool canRedo() const;

		std::string const* undoName() const;

		std::string const* redoName() const;

		bool undo();

		bool redo();

		void markSavePoint();

		bool dirty() const;

		void clear();

		size_t size() const;

		size_t cursor() const;
	};
}
