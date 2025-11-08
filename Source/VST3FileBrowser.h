#pragma once
#include <JuceHeader.h>

class VST3ListBox : public juce::Component,
                    public juce::ListBoxModel,
                    public juce::TextEditor::Listener
{
public:
    VST3ListBox()
    {
        // 검색 박스
        searchBox.setTextToShowWhenEmpty("Search plugins...", juce::Colour(0x80FFDFB9));
        searchBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xffFFDFB9));
        searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
        searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xffA4193D));
        searchBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xffFFDFB9));
        searchBox.addListener(this);
        addAndMakeVisible(searchBox);
        
        // 리스트 박스
        listBox.setModel(this);
        listBox.setRowHeight(30);
        listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a1a1a));
        listBox.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(listBox);
        
        scanPlugins();
    }
    
    void resized() override
    {
        auto area = getLocalBounds();
        searchBox.setBounds(area.removeFromTop(35).reduced(5));
        listBox.setBounds(area);
    }
    
    void scanPlugins()
    {
        allPlugins.clear();
        juce::File vst3Folder("/Library/Audio/Plug-Ins/VST3");
        
        if (vst3Folder.exists() && vst3Folder.isDirectory())
        {
            auto files = vst3Folder.findChildFiles(juce::File::findFilesAndDirectories,
                                                   false, "*.vst3");
            for (auto& file : files)
            {
                if (file.getFileExtension() == ".vst3")
                    allPlugins.add(file.getFullPathName());
            }
            allPlugins.sort(true);
        }
        
        updateFilteredList();
    }
    
    void updateFilteredList()
    {
        filteredPlugins.clear();
        juce::String searchText = searchBox.getText().toLowerCase();
        
        for (const auto& plugin : allPlugins)
        {
            juce::File file(plugin);
            juce::String name = file.getFileNameWithoutExtension().toLowerCase();
            
            if (searchText.isEmpty() || name.contains(searchText))
                filteredPlugins.add(plugin);
        }
        
        listBox.updateContent();
    }
    
    void textEditorTextChanged(juce::TextEditor&) override
    {
        updateFilteredList();
    }
    
    int getNumRows() override
    {
        return filteredPlugins.size();
    }
    
    void paintListBoxItem(int rowNumber, juce::Graphics& g,
                         int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(0xffA4193D));
        else
            g.fillAll(juce::Colour(0xff1a1a1a));
        
        if (rowNumber >= filteredPlugins.size())
            return;
        
        // LP 아이콘 그리기
        const float iconSize = height * 0.6f;
        const float iconX = 10.0f;
        const float iconY = (height - iconSize) * 0.5f;
        
        // 외부 원 (레코드)
        g.setColour(rowIsSelected ? juce::Colour(0xffFFDFB9) : juce::Colour(0xffA4193D));
        g.fillEllipse(iconX, iconY, iconSize, iconSize);
        
        // 중심 원
        g.setColour(rowIsSelected ? juce::Colour(0xffA4193D) : juce::Colour(0xff1a1a1a));
        const float centerSize = iconSize * 0.3f;
        const float centerX = iconX + (iconSize - centerSize) * 0.5f;
        const float centerY = iconY + (iconSize - centerSize) * 0.5f;
        g.fillEllipse(centerX, centerY, centerSize, centerSize);
        
        // 텍스트
        juce::File file(filteredPlugins[rowNumber]);
        g.setColour(rowIsSelected ? juce::Colour(0xffFFDFB9) : juce::Colour(0xffFFDFB9));
        g.setFont(height * 0.5f);
        g.drawText(file.getFileNameWithoutExtension(),
                  (int)(iconX + iconSize + 10), 0,
                  width - (int)(iconX + iconSize + 10), height,
                  juce::Justification::centredLeft, true);
    }
    
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < filteredPlugins.size())
        {
            if (onPluginSelected)
                onPluginSelected(filteredPlugins[row]);
        }
    }
    
    juce::String getSelectedPlugin()
    {
        int selectedRow = listBox.getSelectedRow();
        if (selectedRow >= 0 && selectedRow < filteredPlugins.size())
            return filteredPlugins[selectedRow];
        return {};
    }
    
    bool isPluginSelected()
    {
        return listBox.getSelectedRow() >= 0;
    }
    
    std::function<void(const juce::String&)> onPluginSelected;
    
private:
    juce::TextEditor searchBox;
    juce::ListBox listBox;
    juce::StringArray allPlugins;
    juce::StringArray filteredPlugins;
};
