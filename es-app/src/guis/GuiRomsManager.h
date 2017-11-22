#if defined(EXTENSION) && defined(OBSOLETE) //  Copyright (c) 2015 Filipe Azevedo <pasnox@gmail.com>
#pragma once

#include "PlatformId.h"
#include "components/MenuComponent.h"
#include "components/OptionListComponent.h"

class FileData;
class RomsListView;

class GuiRomsManager final : public MenuComponent
{
	friend class RomsListView;

private:
	struct PlatformData
	{
		PlatformIds::PlatformId id;
		std::string name;
		boost::filesystem::path romsPath;
		boost::filesystem::path externalRomsPath;
	};

public:
	GuiRomsManager(Window* window);
	~GuiRomsManager();

	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

	static std::string platformIdExternalRomsKey(PlatformIds::PlatformId platform);
	static boost::filesystem::path platformIdRomsPath(PlatformIds::PlatformId platform);
	static boost::filesystem::path platformIdExternalRomsPath(PlatformIds::PlatformId platform);

private:
	std::shared_ptr<FileData> m_fileData;
	std::shared_ptr<TextComponent> m_defaultRomsPath;
	std::shared_ptr<TextComponent> m_defaultExternalRomsPath;
	std::shared_ptr<OptionListComponent<PlatformIds::PlatformId>> m_platforms;
	std::shared_ptr<TextComponent> m_platformExternalRomsPath;
	bool m_settingsEdited;

	PlatformData currentPlatformData() const;
	void editDefaultRomsPath();
	void editDefaultExternalRomsPath();
	void editCurrentPlatformExternalRomsPath();
	void showCurrentPlatformRomsManager();
};
#endif