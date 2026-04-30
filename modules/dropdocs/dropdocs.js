/**
 * @file
 * DropDocs — collapsible tree navigation.
 *
 * Parent items in the doc tree can be toggled open or closed. The collapsed
 * state is persisted in localStorage. The path to the currently active item
 * is always kept expanded regardless of saved state.
 */
(function ($) {
  'use strict';

  var STORAGE_KEY = 'dropdocs_expanded';

  /**
   * Returns the set of nids the person has manually expanded.
   * Default state is collapsed, so only explicitly opened items are stored.
   *
   * @return {Array.<number>}
   */
  function loadExpanded() {
    try {
      return JSON.parse(localStorage.getItem(STORAGE_KEY) || '[]');
    }
    catch (e) {
      return [];
    }
  }

  /**
   * Persists the current expanded set to localStorage.
   *
   * @param {Array.<number>} nids
   */
  function saveExpanded(nids) {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(nids));
    }
    catch (e) {
      // localStorage unavailable — state simply won't persist.
    }
  }

  /**
   * Returns the nids that must stay expanded regardless of saved state:
   * all ancestor <li> items of the active item, plus the active item itself
   * if it is also a parent (i.e. you are currently reading a parent doc).
   *
   * @return {Array.<number>}
   */
  function activeAncestorNids() {
    var nids = [];
    $('.dropdocs-tree-active').parents('li.dropdocs-has-children').each(function () {
      nids.push($(this).data('nid'));
    });
    // When viewing a parent doc, the active <li> has both classes.
    // .parents() doesn't include the element itself, so add it explicitly.
    $('li.dropdocs-has-children.dropdocs-tree-active').each(function () {
      nids.push($(this).data('nid'));
    });
    return nids;
  }

  Backdrop.behaviors.dropdocsTree = {
    attach: function (context) {
      var $items = $('li.dropdocs-has-children', context);
      if (!$items.length) {
        return;
      }

      var expanded  = loadExpanded();
      var ancestors = activeAncestorNids();

      // All nested trees start hidden by default (CSS). Mark only the items
      // that should be open — active ancestors and manually expanded items.
      $items.each(function () {
        var $li  = $(this);
        var nid  = $li.data('nid');
        var $btn = $li.find('> .dropdocs-tree-item > .dropdocs-toggle');
        if (ancestors.indexOf(nid) !== -1 || expanded.indexOf(nid) !== -1) {
          $li.addClass('dropdocs-open');
          $btn.attr('aria-expanded', 'true');
        }
        else {
          $btn.attr('aria-expanded', 'false');
        }
      });

      // Shared toggle logic — called by both the caret button and the label.
      function toggleItem($li) {
        var nid   = $li.data('nid');
        var $btn  = $li.find('> .dropdocs-tree-item > .dropdocs-toggle');
        var saved = loadExpanded();
        var idx   = saved.indexOf(nid);

        if ($li.hasClass('dropdocs-open')) {
          $li.removeClass('dropdocs-open');
          $btn.attr('aria-expanded', 'false');
          if (idx !== -1) {
            saved.splice(idx, 1);
          }
        }
        else {
          $li.addClass('dropdocs-open');
          $btn.attr('aria-expanded', 'true');
          if (idx === -1) {
            saved.push(nid);
          }
        }

        saveExpanded(saved);
      }

      // Caret button.
      $items.find('> .dropdocs-tree-item > .dropdocs-toggle')
        .once('dropdocs-toggle')
        .on('click', function () {
          toggleItem($(this).closest('li.dropdocs-has-children'));
        });

      // Label click: first click on a collapsed parent expands it (without
      // navigating); once already expanded a click navigates to the doc page.
      // This gives a clear two-step: reveal children, then go to the page.
      $items.find('> .dropdocs-tree-item > a')
        .once('dropdocs-label-click')
        .on('click', function (e) {
          var $li = $(this).closest('li.dropdocs-has-children');
          if (!$li.hasClass('dropdocs-open')) {
            e.preventDefault();
            toggleItem($li);
          }
          // Already expanded — let the link navigate normally.
        });
    }
  };

})(jQuery);
