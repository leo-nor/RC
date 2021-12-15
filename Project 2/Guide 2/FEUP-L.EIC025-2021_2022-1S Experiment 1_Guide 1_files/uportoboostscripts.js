require(['jquery'],function($){$(document).ready(function(){countFrontpage();$('.frontpage-img img').removeClass('fade-out');removeDrawerFrontpage();$('#page-login-index .card-header img').wrap('<a href="../"></a>');setFakeCourseItem();$('.section-modchooser-text').addClass('btn btn-secondary');$('.path-user .editprofile a').addClass('btn btn-secondary');$('.section-navigation.mdl-bottom .mdl-left>a').addClass('btn btn-primary');$('.section-navigation.mdl-bottom .mdl-right>a').addClass('btn btn-primary');setCourseDashMenu('teacherdash');setCourseDashMenu('studentdash');setMenuOnTop();$('#page-header .page-context-header').after($('#shortname-info'));setNewsBlock($('html').attr("lang"));var hideShowButton='<button id="collapseNavigationButton" class="btn btn-link" data-toggle="collapse" '+'data-target="#collapseNavigation" aria-expanded="true" aria-controls="collapseOne">Mostrar/Ocultar</button>';$('#mod_quiz_navblock.block_fake .card-body h5').after(hideShowButton);$('#mod_quiz_navblock.block_fake .card-body .card-text').addClass('collapse show');$('#mod_quiz_navblock.block_fake .card-body .card-text').attr('id','collapseNavigation');$('#page-enrol-instances #user-notifications').append('<div class="alert alert-warning" role="alert">\
        <strong>Atenção!</strong> Não atribuir acesso de <strong>visitante sem chave</strong>, se existirem pautas ou outros documentos com identificação direta de estudantes.\
        <br />Mais info: apoio.elearning@uporto.pt</div>');$('.restore-precheck-notices .alert').removeClass("alert-danger");$('.restore-precheck-notices .alert').addClass("alert-info");$('.restore-rolemappings .detail-pair-value .description').hide()});function countFrontpage(){$('.counter-count#courses').text($('#page-footer').attr('data-courses'));$('.counter-count#users').text($('#page-footer').attr('data-users'));$('.counter-count#activities').text($('#page-footer').attr('data-activities'));$('.counter-count').each(function(){$(this).prop('Counter',0).animate({Counter:$(this).text()},{duration:1500,easing:'swing',step:function(now){$(this).text(Math.ceil(now))}})})}
function removeDrawerFrontpage(){$('.pagelayout-frontpage #nav-drawer').addClass('closed');$('.pagelayout-frontpage #nav-drawer').attr('aria-hidden',!0)}
function setFakeCourseItem(){var array_ids=['myoverview_courses_view_in_progress','myoverview_courses_view_future','myoverview_courses_view_past'];array_ids.forEach(function(element){var n=$('#'+element+' .courses-view-course-item').length;if((n>1)&&(n&1)){$('#'+element+' div[data-region="paging-content-item"]').append('<div class="card mb-3 courses-view-course-item bg-transparent"></div>')}})}
function setCourseDashMenu(dashtype){var $itemname=$('.'+dashtype+'>a').attr('title');$('.'+dashtype+'>a').attr('class','list-group-item list-group-item-action');$('.'+dashtype+'>a').attr('data-key',dashtype);$('#nav-drawer nav.list-group a[data-key="coursehome"]').after($('.'+dashtype+'>a'));$('a[data-key="'+dashtype+'"]>i').wrap('<div class="m-l-0"><div class="media"><span class="media-left"></div></div></div>');$('a[data-key="'+dashtype+'"] i').addClass('icon');$('a[data-key="'+dashtype+'"] span').after('<span class="media-body ">'+$itemname+'</span>')}
function setMenuOnTop(){$('#nav-drawer nav').removeClass('m-t-1');$('#nav-drawer').prepend($('a[data-key="myhome"]').parent());$('#nav-drawer nav:not(:first-child)').addClass('m-t-1');$('#nav-drawer a[data-key="downloadcenter"]').next().addClass('m-t-1')}
function setNewsBlock(lang){var notice='Notices';if(lang=='pt')notice='Avisos';$('#page-my-index .block_news_items.block h5.card-title').html(notice);$('#page-my-index #region-main .block_news_items').addClass('alert alert-warning alert-dismissible fade in');$('#page-my-index #region-main .block_news_items').append('<a href="#" class="close" data-dismiss="alert" aria-label="close" title="close">×</a>')}})