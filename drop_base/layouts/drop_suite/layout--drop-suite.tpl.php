<?php
/**
 * @file
 * Template for the Drop Suite layout.
 *
 * Full-width layout with optional right sidebar for Drop Suite application
 * pages. Content region uses the full width when no sidebar blocks are placed.
 *
 * Regions:
 * - $content['header']:     Site header (logo, site name).
 * - $content['navigation']: Drop Suite primary navigation bar.
 * - $content['top']:        Full-width region above the main columns.
 * - $content['content']:    Main content column.
 * - $content['sidebar']:    Right sidebar column.
 * - $content['bottom']:     Full-width region below the main columns.
 * - $content['footer']:     Site footer.
 */
?>
<div class="layout--drop-suite <?php print implode(' ', $classes); ?>"<?php print backdrop_attributes($attributes); ?>>
  <div id="skip-link">
    <a href="#main-content" class="element-invisible element-focusable"><?php print t('Skip to main content'); ?></a>
  </div>

  <?php if ($content['header']): ?>
    <header class="l-header" role="banner" aria-label="<?php print t('Site header'); ?>">
      <div class="l-header-inner">
        <?php print $content['header']; ?>
      </div>
    </header>
  <?php endif; ?>

  <?php if ($content['navigation']): ?>
    <nav class="l-navigation" role="navigation" aria-label="<?php print t('Drop Suite navigation'); ?>">
      <div class="l-navigation-inner">
        <?php print $content['navigation']; ?>
      </div>
    </nav>
  <?php endif; ?>

  <div class="l-wrapper">
    <div class="l-wrapper-inner">

      <?php if (!empty($content['messages'])): ?>
        <div class="l-messages">
          <?php print $content['messages']; ?>
        </div>
      <?php endif; ?>

      <?php if (!empty($content['top'])): ?>
        <div class="l-top">
          <?php print $content['top']; ?>
        </div>
      <?php endif; ?>

      <?php if ($tabs): ?>
        <div class="l-tabs"><?php print $tabs; ?></div>
      <?php endif; ?>

      <?php if ($action_links): ?>
        <div class="l-action-links"><?php print $action_links; ?></div>
      <?php endif; ?>

      <?php if ($content['sidebar']): ?>
        <div class="l-middle row">
          <main class="l-content col-md-9" role="main" aria-label="<?php print t('Main content'); ?>">
            <a id="main-content"></a>
            <?php print $content['content']; ?>
          </main>
          <aside class="l-sidebar col-md-3" aria-label="<?php print t('Sidebar'); ?>">
            <?php print $content['sidebar']; ?>
          </aside>
        </div>
      <?php else: ?>
        <div class="l-middle">
          <main class="l-content" role="main" aria-label="<?php print t('Main content'); ?>">
            <a id="main-content"></a>
            <?php print $content['content']; ?>
          </main>
        </div>
      <?php endif; ?>

      <?php if (!empty($content['bottom'])): ?>
        <div class="l-bottom">
          <?php print $content['bottom']; ?>
        </div>
      <?php endif; ?>

    </div>
  </div>

  <?php if ($content['footer']): ?>
    <footer class="l-footer" role="contentinfo">
      <div class="l-footer-inner">
        <?php print $content['footer']; ?>
      </div>
    </footer>
  <?php endif; ?>

</div>
