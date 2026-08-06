#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <utility>

#include "mpp/app/CommandStack.h"
#include "mpp/app/DocumentFile.h"
#include "mpp/app/DocumentFoundationTests.h"
#include "mpp/app/DocumentId.h"
#include "mpp/app/DocumentSnapshot.h"

using namespace std;

namespace mpp::app
{
	namespace
	{
		class SetIntCommand final : public EditorCommand
		{
			string mName;
			int* mTarget;
			int mBefore;
			int mAfter;

		public:
			SetIntCommand(string name, int* target, int after)
				: mName(move(name)), mTarget(target), mBefore(*target), mAfter(after) {}

			string const& name() const override { return mName; }
			void execute() override { *mTarget = mAfter; }
			void undo() override { *mTarget = mBefore; }
		};
	}

	bool runDocumentFoundationTests(string* failure)
	{
		auto fail = [&](char const* message)
		{
			if (failure) *failure = message;
			return false;
		};

		if (!isValidDocumentId("Pass.Bloom-1") || isValidDocumentId("1Pass") || isValidDocumentId("Pass Bloom"))
			return fail("document ID validation is incorrect");
		DocumentIdRegistry ids;
		if (!ids.reserve("Pass") || ids.reserve("Pass") || ids.makeUnique("Pass") != "Pass.2")
			return fail("document ID uniqueness is incorrect");

		DocumentGeneration generations;
		auto value = make_shared<string const>("snapshot");
		DocumentSnapshot<string> snapshot(generations.next(), value);
		if (!snapshot || !generations.isCurrent(snapshot.generation()) || *snapshot.value() != "snapshot")
			return fail("immutable document snapshot generation is incorrect");
		generations.next();
		if (generations.isCurrent(snapshot.generation())) return fail("stale snapshot was accepted as current");

		int number = 0;
		CommandStack commands(3);
		commands.execute(make_unique<SetIntCommand>("One", &number, 1));
		commands.markSavePoint();
		commands.execute(make_unique<SetIntCommand>("Two", &number, 2));
		if (number != 2 || !commands.dirty() || !commands.canUndo()) return fail("command execution state is incorrect");
		commands.undo();
		if (number != 1 || commands.dirty() || !commands.canRedo()) return fail("command undo/save-point state is incorrect");
		commands.redo();
		if (number != 2) return fail("command redo state is incorrect");
		commands.undo();
		commands.execute(make_unique<SetIntCommand>("Branch", &number, 3));
		if (commands.canRedo() || !commands.dirty()) return fail("command branch did not discard redo/save state correctly");

		auto root = filesystem::temp_directory_path() / "mpp-document-foundation-tests";
		auto document = root / "documents" / "pipeline.xml";
		auto target = root / "assets" / "texture.png";
		auto relative = makeDocumentRelativeReference(document, target);
		if (resolveDocumentReference(document, relative) != normaliseDocumentPath(target))
			return fail("document-relative path round trip failed");
		atomicWriteText(document, "first");
		atomicWriteText(document, "second");
		ifstream input(document, ios::binary);
		string contents((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
		error_code ignored;
		filesystem::remove_all(root, ignored);
		if (contents != "second") return fail("atomic document replacement failed");

		return true;
	}
}
