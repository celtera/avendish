// Populate the sidebar
//
// This is a script, and not included directly in the page, to control the total size of the book.
// The TOC contains an entry for each page, so if each page includes a copy of the TOC,
// the total size of the page becomes O(n**2).
class MDBookSidebarScrollbox extends HTMLElement {
    constructor() {
        super();
    }
    connectedCallback() {
        this.innerHTML = '<ol class="chapter"><li class="chapter-item expanded "><a href="foreword.html"><strong aria-hidden="true">1.</strong> Foreword</a></li><li class="chapter-item expanded affix "><li class="part-title">Getting started</li><li class="chapter-item expanded "><a href="getting_started/hello_world.html"><strong aria-hidden="true">2.</strong> Hello World</a></li><li class="chapter-item expanded "><a href="getting_started/compiling.html"><strong aria-hidden="true">3.</strong> Compiling</a></li><li class="chapter-item expanded "><a href="getting_started/running.html"><strong aria-hidden="true">4.</strong> Running</a></li><li class="chapter-item expanded affix "><li class="part-title">Writing CPU processors</li><li class="chapter-item expanded "><a href="writing_processors/ports.html"><strong aria-hidden="true">5.</strong> Adding ports</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="writing_processors/ports.refactoring.html"><strong aria-hidden="true">5.1.</strong> Simplifying ports</a></li><li class="chapter-item expanded "><a href="writing_processors/ports.helpers.html"><strong aria-hidden="true">5.2.</strong> Helpers for ports</a></li><li class="chapter-item expanded "><a href="writing_processors/ports.metadatas.html"><strong aria-hidden="true">5.3.</strong> Port metadatas</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="writing_processors/ports.metadatas.range.html"><strong aria-hidden="true">5.3.1.</strong> Range</a></li><li class="chapter-item expanded "><a href="writing_processors/ports.metadatas.widget.html"><strong aria-hidden="true">5.3.2.</strong> Widget</a></li><li class="chapter-item expanded "><a href="writing_processors/ports.metadatas.mapping.html"><strong aria-hidden="true">5.3.3.</strong> Mapping</a></li><li class="chapter-item expanded "><a href="writing_processors/ports.metadatas.smooth.html"><strong aria-hidden="true">5.3.4.</strong> Smoothing</a></li><li class="chapter-item expanded "><a href="writing_processors/ports.metadatas.helpers.html"><strong aria-hidden="true">5.3.5.</strong> Helpers</a></li></ol></li><li class="chapter-item expanded "><a href="writing_processors/ports.update.html"><strong aria-hidden="true">5.4.</strong> Update callback</a></li></ol></li><li class="chapter-item expanded "><a href="writing_processors/audio.html"><strong aria-hidden="true">6.</strong> Audio</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="writing_processors/audio.polyphonic.html"><strong aria-hidden="true">6.1.</strong> Monophonic processors</a></li><li class="chapter-item expanded "><a href="writing_processors/audio.setup.html"><strong aria-hidden="true">6.2.</strong> Setup</a></li><li class="chapter-item expanded "><a href="writing_processors/audio.arguments.html"><strong aria-hidden="true">6.3.</strong> Playback state</a></li><li class="chapter-item expanded "><a href="writing_processors/audio.fft.html"><strong aria-hidden="true">6.4.</strong> FFT</a></li></ol></li><li class="chapter-item expanded "><a href="writing_processors/messages.html"><strong aria-hidden="true">7.</strong> Messages</a></li><li class="chapter-item expanded "><a href="writing_processors/callbacks.html"><strong aria-hidden="true">8.</strong> Callbacks</a></li><li class="chapter-item expanded "><a href="writing_processors/init.html"><strong aria-hidden="true">9.</strong> Initialization</a></li><li class="chapter-item expanded "><a href="writing_processors/midi.html"><strong aria-hidden="true">10.</strong> MIDI</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="writing_processors/midi.example.html"><strong aria-hidden="true">10.1.</strong> Example</a></li></ol></li><li class="chapter-item expanded "><a href="writing_processors/images.html"><strong aria-hidden="true">11.</strong> Image</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="writing_processors/images.example.html"><strong aria-hidden="true">11.1.</strong> Example</a></li></ol></li><li class="chapter-item expanded "><a href="writing_processors/metadatas.html"><strong aria-hidden="true">12.</strong> Metadatas</a></li><li class="chapter-item expanded affix "><li class="part-title">Advanced features</li><li class="chapter-item expanded "><a href="advanced/port_types.html"><strong aria-hidden="true">13.</strong> Port data types</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="advanced/port_types.example.html"><strong aria-hidden="true">13.1.</strong> Example</a></li><li class="chapter-item expanded "><a href="advanced/port_types.file.html"><strong aria-hidden="true">13.2.</strong> Files</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="advanced/port_types.file.sound.html"><strong aria-hidden="true">13.2.1.</strong> Sound files</a></li><li class="chapter-item expanded "><a href="advanced/port_types.file.midi.html"><strong aria-hidden="true">13.2.2.</strong> MIDI files</a></li><li class="chapter-item expanded "><a href="advanced/port_types.file.raw.html"><strong aria-hidden="true">13.2.3.</strong> Raw files</a></li></ol></li><li class="chapter-item expanded "><a href="advanced/port_types.codeedit.html"><strong aria-hidden="true">13.3.</strong> Code editor</a></li></ol></li><li class="chapter-item expanded "><a href="advanced/ui.html"><strong aria-hidden="true">14.</strong> Custom UI</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="advanced/ui.layout.html"><strong aria-hidden="true">14.1.</strong> Declarative layouts</a></li><li class="chapter-item expanded "><a href="advanced/ui.painting.html"><strong aria-hidden="true">14.2.</strong> Custom items</a></li><li class="chapter-item expanded "><a href="advanced/ui.messages.html"><strong aria-hidden="true">14.3.</strong> Message bus</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="advanced/ui.messages.example.html"><strong aria-hidden="true">14.3.1.</strong> Example</a></li></ol></li></ol></li><li class="chapter-item expanded "><a href="advanced/injection.html"><strong aria-hidden="true">15.</strong> Feature injection</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="advanced/logging.html"><strong aria-hidden="true">15.1.</strong> Logging</a></li><li class="chapter-item expanded "><a href="advanced/fft.html"><strong aria-hidden="true">15.2.</strong> FFT</a></li></ol></li><li class="chapter-item expanded "><a href="advanced/presets.html"><strong aria-hidden="true">16.</strong> Presets</a></li><li class="chapter-item expanded "><a href="advanced/sample_accurate.html"><strong aria-hidden="true">17.</strong> Sample-accurate processing</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="advanced/sample_accurate.example.html"><strong aria-hidden="true">17.1.</strong> Example</a></li></ol></li><li class="chapter-item expanded "><a href="advanced/channel_mimicking.html"><strong aria-hidden="true">18.</strong> Channel mimicking</a></li><li class="chapter-item expanded "><a href="advanced/workers.html"><strong aria-hidden="true">19.</strong> Threaded workers</a></li><li class="chapter-item expanded "><a href="advanced/cmake.html"><strong aria-hidden="true">20.</strong> CMake configuration</a></li><li class="chapter-item expanded affix "><li class="part-title">Writing GPU processors</li><li class="chapter-item expanded "><a href="gpu/draw.html"><strong aria-hidden="true">21.</strong> Draw nodes</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="gpu/draw.layout.html"><strong aria-hidden="true">21.1.</strong> Defining a layout</a></li><li class="chapter-item expanded "><a href="gpu/draw.calls.html"><strong aria-hidden="true">21.2.</strong> API calls</a></li><li class="chapter-item expanded "><a href="gpu/draw.minimal.html"><strong aria-hidden="true">21.3.</strong> Minimal pipeline</a></li><li class="chapter-item expanded "><a href="gpu/draw.example.html"><strong aria-hidden="true">21.4.</strong> Complete example</a></li></ol></li><li class="chapter-item expanded "><a href="gpu/compute.html"><strong aria-hidden="true">22.</strong> Compute nodes</a></li><li class="chapter-item expanded affix "><li class="part-title">Development</li><li class="chapter-item expanded "><a href="development/predicates.html"><strong aria-hidden="true">23.</strong> Type predicates</a></li><li class="chapter-item expanded "><a href="development/flags.html"><strong aria-hidden="true">24.</strong> Flags</a></li><li class="chapter-item expanded "><a href="development/new_concepts.html"><strong aria-hidden="true">25.</strong> Implementing new concepts</a></li></ol>';
        // Set the current, active page, and reveal it if it's hidden
        let current_page = document.location.href.toString();
        if (current_page.endsWith("/")) {
            current_page += "index.html";
        }
        var links = Array.prototype.slice.call(this.querySelectorAll("a"));
        var l = links.length;
        for (var i = 0; i < l; ++i) {
            var link = links[i];
            var href = link.getAttribute("href");
            if (href && !href.startsWith("#") && !/^(?:[a-z+]+:)?\/\//.test(href)) {
                link.href = path_to_root + href;
            }
            // The "index" page is supposed to alias the first chapter in the book.
            if (link.href === current_page || (i === 0 && path_to_root === "" && current_page.endsWith("/index.html"))) {
                link.classList.add("active");
                var parent = link.parentElement;
                if (parent && parent.classList.contains("chapter-item")) {
                    parent.classList.add("expanded");
                }
                while (parent) {
                    if (parent.tagName === "LI" && parent.previousElementSibling) {
                        if (parent.previousElementSibling.classList.contains("chapter-item")) {
                            parent.previousElementSibling.classList.add("expanded");
                        }
                    }
                    parent = parent.parentElement;
                }
            }
        }
        // Track and set sidebar scroll position
        this.addEventListener('click', function(e) {
            if (e.target.tagName === 'A') {
                sessionStorage.setItem('sidebar-scroll', this.scrollTop);
            }
        }, { passive: true });
        var sidebarScrollTop = sessionStorage.getItem('sidebar-scroll');
        sessionStorage.removeItem('sidebar-scroll');
        if (sidebarScrollTop) {
            // preserve sidebar scroll position when navigating via links within sidebar
            this.scrollTop = sidebarScrollTop;
        } else {
            // scroll sidebar to current active section when navigating via "next/previous chapter" buttons
            var activeSection = document.querySelector('#sidebar .active');
            if (activeSection) {
                activeSection.scrollIntoView({ block: 'center' });
            }
        }
        // Toggle buttons
        var sidebarAnchorToggles = document.querySelectorAll('#sidebar a.toggle');
        function toggleSection(ev) {
            ev.currentTarget.parentElement.classList.toggle('expanded');
        }
        Array.from(sidebarAnchorToggles).forEach(function (el) {
            el.addEventListener('click', toggleSection);
        });
    }
}
window.customElements.define("mdbook-sidebar-scrollbox", MDBookSidebarScrollbox);
