package com.iisacc.iigui;

import android.app.Activity;
import android.os.Handler;
import android.os.HandlerThread;
import android.text.InputType;
import android.text.Editable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/** OS input-method endpoint only; iiGUI retains all editing and rendering. */
public final class AndroidImeBridge {
    private static final int READ_ONLY = 1, MULTILINE = 2, PASSWORD = 4, MAX_READ = 65535;
    private static final int SELECT = 1, COMPOSE = 2, COMMIT = 3, FINISH = 4, DELETE = 5, DELETE_CODEPOINTS = 6, REGION = 7;
    private static final Map<Long, WeakReference<ImeView>> VIEWS = new HashMap<>();

    private static native String nativeRead(long token, long generation, int query, int count);
    private static native boolean nativeCommand(long token, long generation, int operation,
                                                String text, int first, int second);

    private AndroidImeBridge() {}

    public static void attach(final Activity activity, final long token) {
        activity.runOnUiThread(new Runnable() {
            @Override public void run() {
                if (activity.isFinishing() || VIEWS.containsKey(token)) return;
                View root = activity.findViewById(android.R.id.content);
                if (!(root instanceof ViewGroup)) return;
                ImeView view = new ImeView(activity, token);
                ((ViewGroup) root).addView(view, new FrameLayout.LayoutParams(1, 1, Gravity.TOP | Gravity.LEFT));
                VIEWS.put(token, new WeakReference<>(view));
            }
        });
    }

    public static void update(final Activity activity, final long token, final long generation,
                              final int length, final int anchor, final int caret,
                              final int markedStart, final int markedEnd, final int flags, final boolean notifySelection) {
        activity.runOnUiThread(new Runnable() {
            @Override public void run() {
                WeakReference<ImeView> ref = VIEWS.get(token);
                ImeView view = ref == null ? null : ref.get();
                if (view != null) view.update(generation, length, anchor, caret, markedStart, markedEnd, flags, notifySelection);
            }
        });
    }

    public static void detach(final Activity activity, final long token) {
        activity.runOnUiThread(new Runnable() {
            @Override public void run() {
                WeakReference<ImeView> ref = VIEWS.remove(token);
                ImeView view = ref == null ? null : ref.get();
                if (view != null) view.dispose();
            }
        });
    }

    private static final class ImeView extends View {
        private final HandlerThread inputThread = new HandlerThread("iiGUI-IME");
        private final Handler handler;
        private final InputMethodManager manager;
        private volatile long token, generation;
        private int length, anchor, caret, markedStart = -1, markedEnd = -1;
        private volatile int flags;
        private WeakReference<View> previousFocus;
        private Connection connection;

        ImeView(Activity activity, long token) {
            super(activity);
            this.token = token;
            inputThread.start();handler = new Handler(inputThread.getLooper());
            manager = (InputMethodManager) activity.getSystemService(Activity.INPUT_METHOD_SERVICE);
            setFocusable(true);setFocusableInTouchMode(true);setClickable(false);
            setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        }
        @Override public boolean onCheckIsTextEditor() { return token != 0 && generation != 0 && (flags & READ_ONLY) == 0; }
        @Override public InputConnection onCreateInputConnection(EditorInfo info) {
            synchronized (this) {
                if (!onCheckIsTextEditor()) return null;
                info.inputType = InputType.TYPE_CLASS_TEXT;
                if ((flags & PASSWORD) != 0) info.inputType |= InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                else if ((flags & MULTILINE) != 0) info.inputType |= InputType.TYPE_TEXT_FLAG_MULTI_LINE;
                info.imeOptions = EditorInfo.IME_ACTION_NONE;
                info.initialSelStart = anchor;info.initialSelEnd = caret;
                connection = new Connection(this, generation);return connection;
            }
        }
        void update(long nextGeneration,int nextLength,int nextAnchor,int nextCaret,int nextMarkedStart,int nextMarkedEnd,int nextFlags,boolean notifySelection) {
            final boolean active=nextGeneration!=0&&(nextFlags&READ_ONLY)==0;
            boolean restart;
            synchronized(this){
                restart=generation!=nextGeneration||flags!=nextFlags;
                generation=nextGeneration;length=nextLength;anchor=nextAnchor;caret=nextCaret;
                markedStart=nextMarkedStart;markedEnd=nextMarkedEnd;flags=nextFlags;
                if(restart&&connection!=null){connection.closed=true;connection=null;}
            }
            if(manager==null)return;
            if(!active){
                manager.hideSoftInputFromWindow(getWindowToken(),0);clearFocus();
                View previous=previousFocus==null?null:previousFocus.get();if(previous!=null)previous.requestFocus();previousFocus=null;return;
            }
            if(!hasFocus()){
                View previous=getRootView().findFocus();if(previous!=null&&previous!=this)previousFocus=new WeakReference<>(previous);
                if(!requestFocus())return;restart=true;
            }
            if(restart){manager.restartInput(this);manager.showSoftInput(this,0);}
            if(notifySelection)publishSelection();
        }
        void publishSelection(){
            if(manager==null||token==0||generation==0)return;
            int a,c,m,n;
            synchronized(this){
                if(connection!=null&&connection.batches>0){connection.pendingSelection=true;return;}
                a=anchor;c=caret;m=markedStart;n=markedEnd;
            }
            manager.updateSelection(this,a,c,m,n);
        }
        void dispose(){
            synchronized(this){token=0;generation=0;if(connection!=null)connection.closed=true;connection=null;}
            if(manager!=null)manager.hideSoftInputFromWindow(getWindowToken(),0);
            if(getParent() instanceof ViewGroup)((ViewGroup)getParent()).removeView(this);
            inputThread.quitSafely();
        }
    }

    private static final class Connection extends BaseInputConnection {
        private final ImeView view;
        private final long generation;
        private volatile boolean closed;
        private int batches;
        private boolean pendingSelection;

        Connection(ImeView view,long generation){super(view,true);this.view=view;this.generation=generation;}
        @Override public Editable getEditable(){return null;}
        @Override public Handler getHandler(){return view.handler;}
        private boolean valid(){return !closed&&view.token!=0&&view.generation==generation;}
        private String read(int query,int count){return valid()?nativeRead(view.token,generation,query,count):null;}
        private boolean command(int op,String text,int first,int second){
            return valid()&&nativeCommand(view.token,generation,op,text,first,second);
        }
        @Override public boolean beginBatchEdit(){synchronized(view){if(!valid()||batches>=32)return false;++batches;return true;}}
        @Override public boolean endBatchEdit(){
            boolean publish=false,remaining;
            synchronized(view){if(!valid()||batches==0)return false;--batches;remaining=batches>0;if(!remaining&&pendingSelection){pendingSelection=false;publish=true;}}
            if(publish)view.post(new Runnable(){@Override public void run(){view.publishSelection();}});return remaining;
        }
        @Override public void closeConnection(){synchronized(view){closed=true;batches=0;pendingSelection=false;}super.closeConnection();}
        @Override public boolean setSelection(int start,int end){return command(SELECT,null,start,end);}
        @Override public boolean setComposingText(CharSequence text,int cursor){return text!=null&&command(COMPOSE,text.toString(),cursor,0);}
        @Override public boolean commitText(CharSequence text,int cursor){return text!=null&&command(COMMIT,text.toString(),cursor,0);}
        @Override public boolean finishComposingText(){return command(FINISH,null,0,0);}
        @Override public boolean deleteSurroundingText(int before,int after){return command(DELETE,null,before,after);}
        @Override public boolean deleteSurroundingTextInCodePoints(int before,int after){return command(DELETE_CODEPOINTS,null,before,after);}
        @Override public boolean setComposingRegion(int start,int end){return command(REGION,null,start,end);}
        @Override public boolean requestCursorUpdates(int mode){return false;}
        @Override public boolean performEditorAction(int action){return false;}
        @Override public CharSequence getSelectedText(int flags){
            return read(0,MAX_READ);
        }
        @Override public CharSequence getTextBeforeCursor(int count,int flags){
            if(count<0)throw new IllegalArgumentException("Negative text length");return read(1,count);
        }
        @Override public CharSequence getTextAfterCursor(int count,int flags){
            if(count<0)throw new IllegalArgumentException("Negative text length");return read(2,count);
        }
    }
}
