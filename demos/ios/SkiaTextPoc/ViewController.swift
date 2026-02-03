import UIKit

class ViewController: UIViewController, UIGestureRecognizerDelegate, UIColorPickerViewControllerDelegate {
    private var skiaView: SkiaMetalView!
    private var heightLabel: UILabel!
    private var widthLabel: UILabel!
    private var lineCountLabel: UILabel!
    private var maxIntrinsicWidthLabel: UILabel!
    private var minIntrinsicWidthLabel: UILabel!
    private var maxWidthSlider: UISlider!
    private var maxWidthValueLabel: UILabel!
    private var currentMaxWidth: Float = 700.0
    
    // Format toolbar controls
    private var formatToolbar: UIStackView!
    private var fontSegment: UISegmentedControl!
    private var fontSizeStepper: UIStepper!
    private var fontSizeLabel: UILabel!
    private var colorButton: UIButton!
    private var boldButton: UIButton!
    private var italicButton: UIButton!
    private var underlineButton: UIButton!
    private var letterSpacingLabel: UILabel!
    private var letterSpacingStepper: UIStepper!
    private var wordSpacingLabel: UILabel!
    private var wordSpacingStepper: UIStepper!
    private var highlightToggleButton: UIButton!
    private var highlightColorButton: UIButton!
    private var shadowToggleButton: UIButton!
    
    private var alignmentSegment: UISegmentedControl!
    private var maxLinesStepper: UIStepper!
    private var maxLinesLabel: UILabel!
    private var ellipsisField: UITextField!
    private var lineHeightStepper: UIStepper!
    private var lineHeightLabel: UILabel!
    
    // Current text color (ARGB format)
    private var currentColor: UInt32 = 0xFF000000
    private var currentHighlightColor: UInt32 = 0xFFFFFF00
    private var shadowColor: UInt32 = 0x80000000
    private var shadowOffsetX: Float = 1.0
    private var shadowOffsetY: Float = 1.0
    private var shadowBlurSigma: Float = 2.0
    private var colorPickerTarget: ColorPickerTarget = .text

    private enum ColorPickerTarget {
        case text
        case highlight
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        view.backgroundColor = .systemBackground
        
        // Tap gesture to dismiss keyboard when tapping outside the text view
        let tapGesture = UITapGestureRecognizer(target: self, action: #selector(dismissKeyboard))
        tapGesture.cancelsTouchesInView = false
        tapGesture.delegate = self
        view.addGestureRecognizer(tapGesture)
        
        // Main scroll view to allow scrolling in landscape mode
        let mainScrollView = UIScrollView()
        mainScrollView.translatesAutoresizingMaskIntoConstraints = false
        mainScrollView.showsVerticalScrollIndicator = true
        mainScrollView.alwaysBounceVertical = true
        view.addSubview(mainScrollView)
        
        // Content view inside main scroll view
        let contentView = UIView()
        contentView.translatesAutoresizingMaskIntoConstraints = false
        mainScrollView.addSubview(contentView)
        
        // Title
        let titleLabel = UILabel()
        titleLabel.text = "Skia Text Rendering PoC"
        titleLabel.font = .systemFont(ofSize: 24, weight: .bold)
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(titleLabel)
        
        let subtitleLabel = UILabel()
        subtitleLabel.text = "Cross-platform text rendering (iOS/Metal)"
        subtitleLabel.font = .systemFont(ofSize: 14)
        subtitleLabel.textColor = .secondaryLabel
        subtitleLabel.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(subtitleLabel)
        
        // Format toolbar
        formatToolbar = UIStackView()
        formatToolbar.axis = .vertical
        formatToolbar.spacing = 8
        formatToolbar.alignment = .leading
        formatToolbar.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(formatToolbar)
        
        let toolbarRow1 = UIStackView()
        toolbarRow1.axis = .horizontal
        toolbarRow1.spacing = 8
        toolbarRow1.alignment = .center
        formatToolbar.addArrangedSubview(toolbarRow1)
        
        let toolbarRow2 = UIStackView()
        toolbarRow2.axis = .horizontal
        toolbarRow2.spacing = 8
        toolbarRow2.alignment = .center
        formatToolbar.addArrangedSubview(toolbarRow2)
        
        // Font picker
        fontSegment = UISegmentedControl(items: ["Roboto", "Playfair", "Playfair-I"])
        fontSegment.selectedSegmentIndex = 0
        fontSegment.addTarget(self, action: #selector(fontChanged), for: .valueChanged)
        toolbarRow1.addArrangedSubview(fontSegment)
        
        // Font size
        fontSizeLabel = UILabel()
        fontSizeLabel.text = "24"
        fontSizeLabel.font = .monospacedSystemFont(ofSize: 14, weight: .medium)
        fontSizeLabel.textAlignment = .center
        fontSizeLabel.widthAnchor.constraint(equalToConstant: 30).isActive = true
        toolbarRow1.addArrangedSubview(fontSizeLabel)
        
        fontSizeStepper = UIStepper()
        fontSizeStepper.minimumValue = 8
        fontSizeStepper.maximumValue = 72
        fontSizeStepper.value = 24
        fontSizeStepper.stepValue = 2
        fontSizeStepper.addTarget(self, action: #selector(fontSizeChanged), for: .valueChanged)
        toolbarRow1.addArrangedSubview(fontSizeStepper)
        
        // Color button (opens color picker)
        colorButton = UIButton(type: .system)
        colorButton.backgroundColor = argbToUIColor(currentColor)
        colorButton.layer.cornerRadius = 6
        colorButton.layer.borderWidth = 1
        colorButton.layer.borderColor = UIColor.systemGray4.cgColor
        colorButton.widthAnchor.constraint(equalToConstant: 36).isActive = true
        colorButton.heightAnchor.constraint(equalToConstant: 36).isActive = true
        colorButton.addTarget(self, action: #selector(colorButtonTapped), for: .touchUpInside)
        toolbarRow1.addArrangedSubview(colorButton)
        
        // Bold button
        boldButton = UIButton(type: .system)
        boldButton.setTitle("B", for: .normal)
        boldButton.titleLabel?.font = .boldSystemFont(ofSize: 16)
        boldButton.layer.borderWidth = 1
        boldButton.layer.borderColor = UIColor.systemGray4.cgColor
        boldButton.layer.cornerRadius = 6
        boldButton.widthAnchor.constraint(equalToConstant: 36).isActive = true
        boldButton.heightAnchor.constraint(equalToConstant: 36).isActive = true
        boldButton.addTarget(self, action: #selector(boldToggled), for: .touchUpInside)
        toolbarRow1.addArrangedSubview(boldButton)
        
        // Italic button
        italicButton = UIButton(type: .system)
        italicButton.setTitle("I", for: .normal)
        italicButton.titleLabel?.font = .italicSystemFont(ofSize: 16)
        italicButton.layer.borderWidth = 1
        italicButton.layer.borderColor = UIColor.systemGray4.cgColor
        italicButton.layer.cornerRadius = 6
        italicButton.widthAnchor.constraint(equalToConstant: 36).isActive = true
        italicButton.heightAnchor.constraint(equalToConstant: 36).isActive = true
        italicButton.addTarget(self, action: #selector(italicToggled), for: .touchUpInside)
        toolbarRow1.addArrangedSubview(italicButton)
        
        // Underline button
        underlineButton = UIButton(type: .system)
        let underlineAttr = NSAttributedString(string: "U", attributes: [.underlineStyle: NSUnderlineStyle.single.rawValue])
        underlineButton.setAttributedTitle(underlineAttr, for: .normal)
        underlineButton.layer.borderWidth = 1
        underlineButton.layer.borderColor = UIColor.systemGray4.cgColor
        underlineButton.layer.cornerRadius = 6
        underlineButton.widthAnchor.constraint(equalToConstant: 36).isActive = true
        underlineButton.heightAnchor.constraint(equalToConstant: 36).isActive = true
        underlineButton.addTarget(self, action: #selector(underlineToggled), for: .touchUpInside)
        toolbarRow1.addArrangedSubview(underlineButton)
        
        // Letter spacing
        letterSpacingLabel = UILabel()
        letterSpacingLabel.text = "LS 0"
        letterSpacingLabel.font = .monospacedSystemFont(ofSize: 12, weight: .medium)
        letterSpacingLabel.widthAnchor.constraint(equalToConstant: 48).isActive = true
        toolbarRow2.addArrangedSubview(letterSpacingLabel)
        
        letterSpacingStepper = UIStepper()
        letterSpacingStepper.minimumValue = -5
        letterSpacingStepper.maximumValue = 20
        letterSpacingStepper.stepValue = 0.5
        letterSpacingStepper.value = 0
        letterSpacingStepper.addTarget(self, action: #selector(letterSpacingChanged), for: .valueChanged)
        toolbarRow2.addArrangedSubview(letterSpacingStepper)
        
        // Word spacing
        wordSpacingLabel = UILabel()
        wordSpacingLabel.text = "WS 0"
        wordSpacingLabel.font = .monospacedSystemFont(ofSize: 12, weight: .medium)
        wordSpacingLabel.widthAnchor.constraint(equalToConstant: 48).isActive = true
        toolbarRow2.addArrangedSubview(wordSpacingLabel)
        
        wordSpacingStepper = UIStepper()
        wordSpacingStepper.minimumValue = 0
        wordSpacingStepper.maximumValue = 30
        wordSpacingStepper.stepValue = 0.5
        wordSpacingStepper.value = 0
        wordSpacingStepper.addTarget(self, action: #selector(wordSpacingChanged), for: .valueChanged)
        toolbarRow2.addArrangedSubview(wordSpacingStepper)
        
        // Highlight toggle
        highlightToggleButton = UIButton(type: .system)
        highlightToggleButton.setTitle("HL", for: .normal)
        highlightToggleButton.layer.borderWidth = 1
        highlightToggleButton.layer.borderColor = UIColor.systemGray4.cgColor
        highlightToggleButton.layer.cornerRadius = 6
        highlightToggleButton.widthAnchor.constraint(equalToConstant: 40).isActive = true
        highlightToggleButton.heightAnchor.constraint(equalToConstant: 32).isActive = true
        highlightToggleButton.setTitleColor(.systemBlue, for: .normal)
        highlightToggleButton.addTarget(self, action: #selector(highlightToggled), for: .touchUpInside)
        toolbarRow2.addArrangedSubview(highlightToggleButton)
        
        // Highlight color button
        highlightColorButton = UIButton(type: .system)
        highlightColorButton.backgroundColor = argbToUIColor(currentHighlightColor)
        highlightColorButton.layer.cornerRadius = 6
        highlightColorButton.layer.borderWidth = 1
        highlightColorButton.layer.borderColor = UIColor.systemGray4.cgColor
        highlightColorButton.widthAnchor.constraint(equalToConstant: 36).isActive = true
        highlightColorButton.heightAnchor.constraint(equalToConstant: 32).isActive = true
        highlightColorButton.addTarget(self, action: #selector(highlightColorTapped), for: .touchUpInside)
        toolbarRow2.addArrangedSubview(highlightColorButton)
        
        // Shadow toggle
        shadowToggleButton = UIButton(type: .system)
        shadowToggleButton.setTitle("Shadow", for: .normal)
        shadowToggleButton.layer.borderWidth = 1
        shadowToggleButton.layer.borderColor = UIColor.systemGray4.cgColor
        shadowToggleButton.layer.cornerRadius = 6
        shadowToggleButton.heightAnchor.constraint(equalToConstant: 32).isActive = true
        shadowToggleButton.setTitleColor(.systemBlue, for: .normal)
        shadowToggleButton.addTarget(self, action: #selector(shadowToggled), for: .touchUpInside)
        toolbarRow2.addArrangedSubview(shadowToggleButton)
        
        // Create Skia view. Use fixed 800x400 points to match web's 800x400 CSS pixels
        skiaView = SkiaMetalView(frame: .zero)
        skiaView.translatesAutoresizingMaskIntoConstraints = false
        skiaView.layer.borderWidth = 1
        skiaView.layer.borderColor = UIColor.systemGray4.cgColor
        skiaView.layer.cornerRadius = 8
        skiaView.clipsToBounds = true
        
        // Canvas scroll view (for horizontal scrolling of the canvas)
        let canvasScrollView = UIScrollView()
        canvasScrollView.translatesAutoresizingMaskIntoConstraints = false
        canvasScrollView.showsHorizontalScrollIndicator = true
        canvasScrollView.showsVerticalScrollIndicator = true
        contentView.addSubview(canvasScrollView)
        
        // Add skiaView to canvas scroll view
        canvasScrollView.addSubview(skiaView)
        
        // Create controls container
        let controlsStack = UIStackView()
        controlsStack.axis = .vertical
        controlsStack.spacing = 12
        controlsStack.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(controlsStack)
        
        let controlsTitle = UILabel()
        controlsTitle.text = "CONTROLS"
        controlsTitle.font = .systemFont(ofSize: 12, weight: .semibold)
        controlsTitle.textColor = .systemTeal
        controlsStack.addArrangedSubview(controlsTitle)
        
        // Max width slider row
        let sliderRow = UIStackView()
        sliderRow.axis = .horizontal
        sliderRow.spacing = 12
        sliderRow.alignment = .center
        
        let sliderLabel = UILabel()
        sliderLabel.text = "Max Width:"
        sliderLabel.font = .systemFont(ofSize: 14)
        sliderLabel.setContentHuggingPriority(.required, for: .horizontal)
        sliderRow.addArrangedSubview(sliderLabel)
        
        maxWidthSlider = UISlider()
        maxWidthSlider.minimumValue = 100
        maxWidthSlider.maximumValue = 780
        maxWidthSlider.value = currentMaxWidth
        maxWidthSlider.addTarget(self, action: #selector(sliderChanged), for: .valueChanged)
        sliderRow.addArrangedSubview(maxWidthSlider)
        
        maxWidthValueLabel = UILabel()
        maxWidthValueLabel.text = "\(Int(currentMaxWidth)) px"
        maxWidthValueLabel.font = .monospacedSystemFont(ofSize: 14, weight: .semibold)
        maxWidthValueLabel.textColor = .systemGreen
        maxWidthValueLabel.setContentHuggingPriority(.required, for: .horizontal)
        maxWidthValueLabel.widthAnchor.constraint(equalToConstant: 70).isActive = true
        sliderRow.addArrangedSubview(maxWidthValueLabel)
        
        controlsStack.addArrangedSubview(sliderRow)
        
        // Alignment
        let alignmentRow = UIStackView()
        alignmentRow.axis = .horizontal
        alignmentRow.spacing = 12
        alignmentRow.alignment = .center

        let alignmentLabel = UILabel()
        alignmentLabel.text = "Alignment:"
        alignmentLabel.font = .systemFont(ofSize: 14)
        alignmentLabel.setContentHuggingPriority(.required, for: .horizontal)
        alignmentRow.addArrangedSubview(alignmentLabel)

        alignmentSegment = UISegmentedControl(items: ["Left", "Center", "Right", "Justify"])
        alignmentSegment.selectedSegmentIndex = 0
        alignmentSegment.addTarget(self, action: #selector(alignmentChanged), for: .valueChanged)
        alignmentRow.addArrangedSubview(alignmentSegment)
        controlsStack.addArrangedSubview(alignmentRow)

        // Max lines
        let maxLinesRow = UIStackView()
        maxLinesRow.axis = .horizontal
        maxLinesRow.spacing = 12
        maxLinesRow.alignment = .center

        let maxLinesLabelTitle = UILabel()
        maxLinesLabelTitle.text = "Max Lines:"
        maxLinesLabelTitle.font = .systemFont(ofSize: 14)
        maxLinesLabelTitle.setContentHuggingPriority(.required, for: .horizontal)
        maxLinesRow.addArrangedSubview(maxLinesLabelTitle)

        maxLinesStepper = UIStepper()
        maxLinesStepper.minimumValue = 0
        maxLinesStepper.maximumValue = 20
        maxLinesStepper.stepValue = 1
        maxLinesStepper.value = 0
        maxLinesStepper.addTarget(self, action: #selector(maxLinesChanged), for: .valueChanged)
        maxLinesRow.addArrangedSubview(maxLinesStepper)

        maxLinesLabel = UILabel()
        maxLinesLabel.text = "0"
        maxLinesLabel.font = .monospacedSystemFont(ofSize: 14, weight: .semibold)
        maxLinesLabel.textColor = .systemGreen
        maxLinesLabel.widthAnchor.constraint(equalToConstant: 30).isActive = true
        maxLinesRow.addArrangedSubview(maxLinesLabel)

        controlsStack.addArrangedSubview(maxLinesRow)

        // Ellipsis
        let ellipsisRow = UIStackView()
        ellipsisRow.axis = .horizontal
        ellipsisRow.spacing = 12
        ellipsisRow.alignment = .center

        let ellipsisLabel = UILabel()
        ellipsisLabel.text = "Ellipsis:"
        ellipsisLabel.font = .systemFont(ofSize: 14)
        ellipsisLabel.setContentHuggingPriority(.required, for: .horizontal)
        ellipsisRow.addArrangedSubview(ellipsisLabel)

        ellipsisField = UITextField()
        ellipsisField.borderStyle = .roundedRect
        ellipsisField.text = "..."
        ellipsisField.widthAnchor.constraint(equalToConstant: 80).isActive = true
        ellipsisField.addTarget(self, action: #selector(ellipsisChanged), for: .editingChanged)
        ellipsisRow.addArrangedSubview(ellipsisField)

        controlsStack.addArrangedSubview(ellipsisRow)

        // Line height
        let lineHeightRow = UIStackView()
        lineHeightRow.axis = .horizontal
        lineHeightRow.spacing = 12
        lineHeightRow.alignment = .center

        let lineHeightLabelTitle = UILabel()
        lineHeightLabelTitle.text = "Line Height:"
        lineHeightLabelTitle.font = .systemFont(ofSize: 14)
        lineHeightLabelTitle.setContentHuggingPriority(.required, for: .horizontal)
        lineHeightRow.addArrangedSubview(lineHeightLabelTitle)

        lineHeightStepper = UIStepper()
        lineHeightStepper.minimumValue = 0
        lineHeightStepper.maximumValue = 4
        lineHeightStepper.stepValue = 0.1
        lineHeightStepper.value = 0
        lineHeightStepper.addTarget(self, action: #selector(lineHeightChanged), for: .valueChanged)
        lineHeightRow.addArrangedSubview(lineHeightStepper)

        lineHeightLabel = UILabel()
        lineHeightLabel.text = "0"
        lineHeightLabel.font = .monospacedSystemFont(ofSize: 14, weight: .semibold)
        lineHeightLabel.textColor = .systemGreen
        lineHeightLabel.widthAnchor.constraint(equalToConstant: 40).isActive = true
        lineHeightRow.addArrangedSubview(lineHeightLabel)

        controlsStack.addArrangedSubview(lineHeightRow)

        // Save button
        let saveButton = UIButton(type: .system)
        saveButton.setTitle("💾 Save as PNG", for: .normal)
        saveButton.titleLabel?.font = .systemFont(ofSize: 16, weight: .semibold)
        saveButton.backgroundColor = .systemTeal
        saveButton.setTitleColor(.white, for: .normal)
        saveButton.layer.cornerRadius = 8
        saveButton.heightAnchor.constraint(equalToConstant: 44).isActive = true
        saveButton.addTarget(self, action: #selector(saveImage), for: .touchUpInside)
        controlsStack.addArrangedSubview(saveButton)
        
        // Create metrics container
        let metricsStack = UIStackView()
        metricsStack.axis = .vertical
        metricsStack.spacing = 8
        metricsStack.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(metricsStack)
        
        let metricsTitle = UILabel()
        metricsTitle.text = "LAYOUT METRICS"
        metricsTitle.font = .systemFont(ofSize: 12, weight: .semibold)
        metricsTitle.textColor = .systemTeal
        metricsStack.addArrangedSubview(metricsTitle)
        
        heightLabel = createMetricLabel(title: "Height")
        widthLabel = createMetricLabel(title: "Width")
        lineCountLabel = createMetricLabel(title: "Line Count")
        maxIntrinsicWidthLabel = createMetricLabel(title: "Max Intrinsic Width")
        minIntrinsicWidthLabel = createMetricLabel(title: "Min Intrinsic Width")
        
        metricsStack.addArrangedSubview(heightLabel)
        metricsStack.addArrangedSubview(widthLabel)
        metricsStack.addArrangedSubview(lineCountLabel)
        metricsStack.addArrangedSubview(maxIntrinsicWidthLabel)
        metricsStack.addArrangedSubview(minIntrinsicWidthLabel)
        
        // Layout constraints
        NSLayoutConstraint.activate([
            // Main scroll view fills the safe area
            mainScrollView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            mainScrollView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            mainScrollView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            mainScrollView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),
            
            // Content view fills scroll view and matches width
            contentView.topAnchor.constraint(equalTo: mainScrollView.topAnchor),
            contentView.leadingAnchor.constraint(equalTo: mainScrollView.leadingAnchor),
            contentView.trailingAnchor.constraint(equalTo: mainScrollView.trailingAnchor),
            contentView.bottomAnchor.constraint(equalTo: mainScrollView.bottomAnchor),
            contentView.widthAnchor.constraint(equalTo: mainScrollView.widthAnchor),
            
            titleLabel.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 20),
            titleLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            
            subtitleLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 4),
            subtitleLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            
            // Format toolbar
            formatToolbar.topAnchor.constraint(equalTo: subtitleLabel.bottomAnchor, constant: 12),
            formatToolbar.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            
            // Canvas scroll view
            canvasScrollView.topAnchor.constraint(equalTo: formatToolbar.bottomAnchor, constant: 12),
            canvasScrollView.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            canvasScrollView.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            canvasScrollView.heightAnchor.constraint(equalToConstant: 250),
            
            // Skia view inside canvas scroll view - fixed size to match web
            skiaView.topAnchor.constraint(equalTo: canvasScrollView.topAnchor),
            skiaView.leadingAnchor.constraint(equalTo: canvasScrollView.leadingAnchor),
            skiaView.widthAnchor.constraint(equalToConstant: 800),
            skiaView.heightAnchor.constraint(equalToConstant: 400),
            skiaView.bottomAnchor.constraint(equalTo: canvasScrollView.bottomAnchor),
            skiaView.trailingAnchor.constraint(equalTo: canvasScrollView.trailingAnchor),
            
            controlsStack.topAnchor.constraint(equalTo: canvasScrollView.bottomAnchor, constant: 20),
            controlsStack.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            controlsStack.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            
            metricsStack.topAnchor.constraint(equalTo: controlsStack.bottomAnchor, constant: 20),
            metricsStack.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            metricsStack.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            metricsStack.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -20),
        ])
        
        // Set up cursor change callback to sync toolbar
        skiaView.onCursorChanged = { [weak self] in
            self?.updateToolbarState()
        }
        
        // Initial render (after layout)
        DispatchQueue.main.async { [weak self] in
            self?.renderText()
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                self?.updateMetrics()
                self?.updateToolbarState()
            }
        }
    }
    
    private func renderText() {
        // All values are in logical pixels, so the renderer applies scale transform internally
        let textOffset: Float = 50.0
        skiaView.textOffsetX = textOffset
        skiaView.textOffsetY = textOffset
        skiaView.setRenderPosition(x: textOffset, y: textOffset)
        
        skiaView.setMaxWidth(currentMaxWidth)
        skiaView.beginRichText()
        skiaView.addStyledSpan("Hello, ", fontFamily: "Playfair", fontSize: 32.0, color: 0xFF2196F3)
        skiaView.addStyledSpan("World! ", fontFamily: "Roboto", fontSize: 28.0, color: 0xFFF44336, fontWeight: 700)
        skiaView.addStyledSpan("This is ", fontFamily: "Roboto", fontSize: 24.0, color: 0xFF000000)
        skiaView.addStyledSpan("rich text ", fontFamily: "Playfair", fontSize: 26.0, color: 0xFF4CAF50)
        skiaView.addStyledSpan("with ", fontFamily: "Roboto", fontSize: 24.0, color: 0xFF000000)
        skiaView.addStyledSpan("multiple fonts ", fontFamily: "Playfair-Italic", fontSize: 26.0, color: 0xFF9C27B0)
        skiaView.addStyledSpan("and ", fontFamily: "Roboto", fontSize: 24.0, color: 0xFF000000)
        skiaView.addStyledSpan("styles! ", fontFamily: "Roboto", fontSize: 24.0, color: 0xFFFF9800, underline: true)
        skiaView.addStyledSpan("The quick brown fox jumps over the lazy dog.", fontFamily: "Playfair", fontSize: 22.0, color: 0xFF666666)
        skiaView.addStyledSpan("\nClick to edit. Try typing!", fontFamily: "Playfair", fontSize: 22.0, color: 0xFF666666)

        skiaView.endRichText()
        
        // Move cursor to end initially
        skiaView.moveCursorToDocumentEnd()
    }
    
    @objc private func sliderChanged() {
        currentMaxWidth = maxWidthSlider.value
        maxWidthValueLabel.text = "\(Int(currentMaxWidth)) px"
        // renderText()
        skiaView.setMaxWidth(currentMaxWidth)
        
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) { [weak self] in
            self?.updateMetrics()
        }
    }
    
    @objc private func saveImage() {
        // Save current state
        let hadSelection = skiaView.hasSelection()
        let selection = skiaView.getSelection()
        let cursorPosition = skiaView.getCursorPosition()
        
        // Temporarily hide cursor and clear selection for clean snapshot
        skiaView.setShowCursor(false)
        skiaView.clearSelection()
        
        // Take snapshot
        let image = skiaView.snapshot()
        
        // Restore state
        if hadSelection {
            skiaView.setSelection(start: selection.start, end: selection.end)
        } else {
            skiaView.setCursorPosition(cursorPosition)
        }
        skiaView.setShowCursor(skiaView.isFirstResponder)
        
        guard let image = image else {
            showAlert(title: "Error", message: "Failed to capture image")
            return
        }
        
        UIImageWriteToSavedPhotosAlbum(image, self, #selector(imageSaved(_:didFinishSavingWithError:contextInfo:)), nil)
    }
    
    @objc private func imageSaved(_ image: UIImage, didFinishSavingWithError error: Error?, contextInfo: UnsafeRawPointer) {
        if let error = error {
            showAlert(title: "Error", message: error.localizedDescription)
        } else {
            showAlert(title: "Saved", message: "Image saved to Photos")
        }
    }
    
    private func showAlert(title: String, message: String) {
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
    
    private func createMetricLabel(title: String) -> UILabel {
        let label = UILabel()
        label.font = .monospacedSystemFont(ofSize: 14, weight: .regular)
        label.text = "\(title): -"
        return label
    }
    
    private func updateMetrics() {
        // Metrics are in logical pixels (scale transform is applied in renderer)
        heightLabel.text = "Height: \(String(format: "%.2f", skiaView.getHeight())) px"
        widthLabel.text = "Width: \(String(format: "%.2f", skiaView.getWidth())) px"
        lineCountLabel.text = "Line Count: \(skiaView.getLineCount())"
        maxIntrinsicWidthLabel.text = "Max Intrinsic Width: \(String(format: "%.2f", skiaView.getMaxIntrinsicWidth())) px"
        minIntrinsicWidthLabel.text = "Min Intrinsic Width: \(String(format: "%.2f", skiaView.getMinIntrinsicWidth())) px"
        
        print("Skia Text PoC - iOS Metrics (logical px):")
        print("  Height: \(skiaView.getHeight())")
        print("  Width: \(skiaView.getWidth())")
        print("  Line Count: \(skiaView.getLineCount())")
        print("  Max Intrinsic Width: \(skiaView.getMaxIntrinsicWidth())")
        print("  Min Intrinsic Width: \(skiaView.getMinIntrinsicWidth())")
    }
    
    private func updateToolbarState() {
        let style = skiaView.getStyleAtCursor()
        guard !style.isEmpty else { return }
        
        // Update font segment
        let fontFamily = style["fontFamily"] as? String ?? "Roboto"
        let fonts = ["Roboto", "Playfair", "Playfair-Italic"]
        if let index = fonts.firstIndex(of: fontFamily) {
            fontSegment.selectedSegmentIndex = index
        }
        
        // Update font size (already in logical pixels)
        let fontSize = (style["fontSize"] as? NSNumber)?.floatValue ?? 24
        fontSizeStepper.value = Double(fontSize)
        fontSizeLabel.text = "\(Int(fontSize))"
        
        // Update color button with actual color from cursor
        let color = (style["color"] as? NSNumber)?.uint32Value ?? 0xFF000000
        currentColor = color
        colorButton.backgroundColor = argbToUIColor(color)
        
        // Update bold button
        let fontWeight = (style["fontWeight"] as? NSNumber)?.int32Value ?? 400
        let isBold = fontWeight >= 700
        boldButton.backgroundColor = isBold ? .systemBlue : .clear
        boldButton.setTitleColor(isBold ? .white : .systemBlue, for: .normal)
        
        // Update italic button
        let isItalic = style["italic"] as? Bool ?? false
        italicButton.backgroundColor = isItalic ? .systemBlue : .clear
        italicButton.setTitleColor(isItalic ? .white : .systemBlue, for: .normal)
        
        // Update underline button
        let isUnderline = style["underline"] as? Bool ?? false
        underlineButton.backgroundColor = isUnderline ? .systemBlue : .clear
        let underlineColor: UIColor = isUnderline ? .white : .systemBlue
        let underlineAttr = NSAttributedString(string: "U", attributes: [
            .underlineStyle: NSUnderlineStyle.single.rawValue,
            .foregroundColor: underlineColor
        ])
        underlineButton.setAttributedTitle(underlineAttr, for: .normal)

        // Letter/word spacing
        let letterSpacing = (style["letterSpacing"] as? NSNumber)?.floatValue ?? 0
        let wordSpacing = (style["wordSpacing"] as? NSNumber)?.floatValue ?? 0
        letterSpacingStepper.value = Double(letterSpacing)
        letterSpacingLabel.text = String(format: "LS %.1f", letterSpacing)
        wordSpacingStepper.value = Double(wordSpacing)
        wordSpacingLabel.text = String(format: "WS %.1f", wordSpacing)

        // Highlight
        let hasBackground = (style["hasBackground"] as? Bool) ?? false
        highlightToggleButton.backgroundColor = hasBackground ? .systemYellow : .clear
        highlightToggleButton.setTitleColor(hasBackground ? .black : .systemBlue, for: .normal)
        if hasBackground, let backgroundColor = style["backgroundColor"] as? NSNumber {
            currentHighlightColor = backgroundColor.uint32Value
        }
        highlightColorButton.backgroundColor = argbToUIColor(currentHighlightColor)

        // Shadow
        let hasShadow = (style["hasShadow"] as? Bool) ?? false
        shadowToggleButton.backgroundColor = hasShadow ? .systemBlue : .clear
        shadowToggleButton.setTitleColor(hasShadow ? .white : .systemBlue, for: .normal)
    }
    
    // MARK: - Keyboard Dismissal
    
    @objc private func dismissKeyboard() {
        view.endEditing(true)
    }
    
    // Only dismiss keyboard when tapping outside the skiaView and format toolbar
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldReceive touch: UITouch) -> Bool {
        // If touch is inside skiaView or format toolbar, don't dismiss keyboard
        let touchedView = touch.view
        if touchedView === skiaView || touchedView?.isDescendant(of: skiaView) == true {
            return false
        }
        if touchedView === formatToolbar || touchedView?.isDescendant(of: formatToolbar) == true {
            return false
        }
        return true
    }
    
    // MARK: - Format Toolbar Actions
    
    // Get style from toolbar controls (reflects user's cumulative choices)
    private func getToolbarStyle() -> (fontFamily: String, fontSize: Float, color: UInt32, fontWeight: Int32, italic: Bool, underline: Bool, letterSpacing: Float, wordSpacing: Float, hasBackground: Bool, backgroundColor: UInt32, hasShadow: Bool) {
        let fonts = ["Roboto", "Playfair", "Playfair-Italic"]
        return (
            fontFamily: fonts[fontSegment.selectedSegmentIndex],
            fontSize: Float(fontSizeStepper.value),
            color: currentColor,
            fontWeight: boldButton.backgroundColor == .systemBlue ? 700 : 400,
            italic: italicButton.backgroundColor == .systemBlue,
            underline: underlineButton.backgroundColor == .systemBlue,
            letterSpacing: Float(letterSpacingStepper.value),
            wordSpacing: Float(wordSpacingStepper.value),
            hasBackground: highlightToggleButton.backgroundColor == .systemYellow,
            backgroundColor: currentHighlightColor,
            hasShadow: shadowToggleButton.backgroundColor == .systemBlue
        )
    }
    
    private func applyStyle(fontFamily: String, fontSize: Float, color: UInt32, fontWeight: Int32, italic: Bool, underline: Bool, letterSpacing: Float, wordSpacing: Float, hasBackground: Bool, backgroundColor: UInt32, hasShadow: Bool) {
        // Keep keyboard visible
        if !skiaView.isFirstResponder {
            _ = skiaView.becomeFirstResponder()
        }
        
        if skiaView.hasSelection() {
            skiaView.applyStyleToSelection(fontFamily: fontFamily, fontSize: fontSize, color: color,
                                           fontWeight: fontWeight, italic: italic, underline: underline,
                                           letterSpacing: letterSpacing, wordSpacing: wordSpacing,
                                           backgroundColor: backgroundColor, hasBackground: hasBackground,
                                           shadowColor: shadowColor, shadowOffsetX: shadowOffsetX, shadowOffsetY: shadowOffsetY,
                                           shadowBlurSigma: shadowBlurSigma, hasShadow: hasShadow)
        } else {
            // Set typing style for next input on the skiaView
            skiaView.typingStyle = [
                "fontFamily": fontFamily,
                "fontSize": fontSize,
                "color": color,
                "fontWeight": fontWeight,
                "italic": italic,
                "underline": underline,
                "letterSpacing": letterSpacing,
                "wordSpacing": wordSpacing,
                "hasBackground": hasBackground,
                "backgroundColor": backgroundColor,
                "hasShadow": hasShadow,
                "shadowColor": shadowColor,
                "shadowOffsetX": shadowOffsetX,
                "shadowOffsetY": shadowOffsetY,
                "shadowBlurSigma": shadowBlurSigma
            ]
        }
    }

    private func applyToolbarStyle() {
        let style = getToolbarStyle()
        applyStyle(fontFamily: style.fontFamily, fontSize: style.fontSize, color: style.color,
                   fontWeight: style.fontWeight, italic: style.italic, underline: style.underline,
                   letterSpacing: style.letterSpacing, wordSpacing: style.wordSpacing,
                   hasBackground: style.hasBackground, backgroundColor: style.backgroundColor, hasShadow: style.hasShadow)
    }
    
    @objc private func fontChanged() {
        applyToolbarStyle()
    }
    
    @objc private func fontSizeChanged() {
        fontSizeLabel.text = "\(Int(fontSizeStepper.value))"
        applyToolbarStyle()
    }
    
    @objc private func colorButtonTapped() {
        colorPickerTarget = .text
        let colorPicker = UIColorPickerViewController()
        colorPicker.delegate = self
        colorPicker.selectedColor = argbToUIColor(currentColor)
        colorPicker.supportsAlpha = false
        present(colorPicker, animated: true)
    }
    
    // MARK: - UIColorPickerViewControllerDelegate
    
    func colorPickerViewController(_ viewController: UIColorPickerViewController, didSelect color: UIColor, continuously: Bool) {
        switch colorPickerTarget {
        case .text:
            currentColor = uiColorToARGB(color)
            colorButton.backgroundColor = color
        case .highlight:
            currentHighlightColor = uiColorToARGB(color)
            highlightColorButton.backgroundColor = color
        }
        applyToolbarStyle()
    }
    
    // MARK: - Color Conversion Helpers
    
    private func argbToUIColor(_ argb: UInt32) -> UIColor {
        let a = CGFloat((argb >> 24) & 0xFF) / 255.0
        let r = CGFloat((argb >> 16) & 0xFF) / 255.0
        let g = CGFloat((argb >> 8) & 0xFF) / 255.0
        let b = CGFloat(argb & 0xFF) / 255.0
        return UIColor(red: r, green: g, blue: b, alpha: a)
    }
    
    private func uiColorToARGB(_ color: UIColor) -> UInt32 {
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        color.getRed(&r, green: &g, blue: &b, alpha: &a)
        let ar = UInt32(a * 255) << 24
        let rr = UInt32(r * 255) << 16
        let gr = UInt32(g * 255) << 8
        let br = UInt32(b * 255)
        return ar | rr | gr | br
    }
    
    @objc private func boldToggled() {
        let isActive = !(boldButton.backgroundColor == .systemBlue)
        boldButton.backgroundColor = isActive ? .systemBlue : .clear
        boldButton.setTitleColor(isActive ? .white : .systemBlue, for: .normal)
        applyToolbarStyle()
    }
    
    @objc private func italicToggled() {
        let isActive = !(italicButton.backgroundColor == .systemBlue)
        italicButton.backgroundColor = isActive ? .systemBlue : .clear
        italicButton.setTitleColor(isActive ? .white : .systemBlue, for: .normal)
        applyToolbarStyle()
    }
    
    @objc private func underlineToggled() {
        let isActive = !(underlineButton.backgroundColor == .systemBlue)
        underlineButton.backgroundColor = isActive ? .systemBlue : .clear
        let color: UIColor = isActive ? .white : .systemBlue
        let underlineAttr = NSAttributedString(string: "U", attributes: [
            .underlineStyle: NSUnderlineStyle.single.rawValue,
            .foregroundColor: color
        ])
        underlineButton.setAttributedTitle(underlineAttr, for: .normal)
        applyToolbarStyle()
    }

    @objc private func letterSpacingChanged() {
        letterSpacingLabel.text = String(format: "LS %.1f", letterSpacingStepper.value)
        applyToolbarStyle()
    }

    @objc private func wordSpacingChanged() {
        wordSpacingLabel.text = String(format: "WS %.1f", wordSpacingStepper.value)
        applyToolbarStyle()
    }

    @objc private func highlightToggled() {
        let isActive = !(highlightToggleButton.backgroundColor == .systemYellow)
        highlightToggleButton.backgroundColor = isActive ? .systemYellow : .clear
        highlightToggleButton.setTitleColor(isActive ? .black : .systemBlue, for: .normal)
        applyToolbarStyle()
    }

    @objc private func shadowToggled() {
        let isActive = !(shadowToggleButton.backgroundColor == .systemBlue)
        shadowToggleButton.backgroundColor = isActive ? .systemBlue : .clear
        shadowToggleButton.setTitleColor(isActive ? .white : .systemBlue, for: .normal)
        applyToolbarStyle()
    }

    @objc private func highlightColorTapped() {
        colorPickerTarget = .highlight
        let colorPicker = UIColorPickerViewController()
        colorPicker.delegate = self
        colorPicker.selectedColor = argbToUIColor(currentHighlightColor)
        colorPicker.supportsAlpha = false
        present(colorPicker, animated: true)
    }

    @objc private func alignmentChanged() {
        let alignment: SkiaTextAlignment
        switch alignmentSegment.selectedSegmentIndex {
        case 1:
            alignment = .center
        case 2:
            alignment = .right
        case 3:
            alignment = .justify
        default:
            alignment = .left
        }
        skiaView.setTextAlignment(alignment)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) { [weak self] in
            self?.updateMetrics()
        }
    }

    @objc private func maxLinesChanged() {
        let maxLines = Int(maxLinesStepper.value)
        maxLinesLabel.text = "\(maxLines)"
        skiaView.setMaxLines(maxLines)
        if maxLines > 0 {
            skiaView.setEllipsis(ellipsisField.text ?? "...")
        } else {
            skiaView.setEllipsis("")
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) { [weak self] in
            self?.updateMetrics()
        }
    }

    @objc private func ellipsisChanged() {
        let maxLines = Int(maxLinesStepper.value)
        if maxLines > 0 {
            skiaView.setEllipsis(ellipsisField.text ?? "...")
        }
    }

    @objc private func lineHeightChanged() {
        lineHeightLabel.text = String(format: "%.1f", lineHeightStepper.value)
        skiaView.setLineHeight(Float(lineHeightStepper.value))
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) { [weak self] in
            self?.updateMetrics()
        }
    }
}
