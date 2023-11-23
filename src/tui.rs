use std::sync::Arc;
use std::io::{stdout, Result};
use std::collections::VecDeque;
use log::Level;

use crossterm::{
    event::{self, KeyCode, KeyEventKind},
    terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
    ExecutableCommand,
};
use ratatui::{
    prelude::*,
    widgets::{Paragraph, Block, Borders},
};

use tokio::sync::mpsc;

use crate::config::Config;

pub async fn tui_main(config: Arc<Config>, cmd_tx: mpsc::Sender<Vec<String>>) {
    let mut tui = Tui::new().expect("Failed to initialize tui!");

    'app: loop {
        if tui.update(&cmd_tx).await.expect("Failed to run tui.update!") {
            cmd_tx.send(vec!["exit".to_string()]).await.unwrap();
            break 'app;
        }
        tui.draw().expect("Failed to run tui.draw!");
    }
}

struct Tui {
    terminal: Terminal<CrosstermBackend<std::io::Stdout>>,

    log_buffer: VecDeque<(Level, String)>,

    history_scroll: usize,
    input_history: Vec<String>,
    input: String,
}

impl Drop for Tui {
    fn drop(&mut self) {
        if let Err(e) = self.cleanup() {
            eprintln!("Error occured related to the TUI: {}", e);
        }
    }
}

impl Tui {
    fn new() -> Result<Self> {
        stdout().execute(EnterAlternateScreen)?;
        enable_raw_mode()?;

        let terminal = Terminal::new(CrosstermBackend::new(stdout()))?;

        Ok(Self {
            terminal,

            log_buffer: VecDeque::new(),

            history_scroll: 0,
            input_history: Vec::new(),
            input: String::new(),
        })
    }

    fn cleanup(&self) -> Result<()> {
        stdout().execute(LeaveAlternateScreen)?;
        disable_raw_mode()?;
        Ok(())
    }

    async fn update(&mut self, cmd_tx: &mpsc::Sender<Vec<String>>) -> Result<bool> {
        for (level, msg) in crate::logger::drain_log_buffer().await {
            self.log_buffer.push_front((level, msg));
            if self.log_buffer.len() > 100 {
                self.log_buffer.pop_back();
            }
        }

        if event::poll(std::time::Duration::from_millis(16))? {
            if let event::Event::Key(key) = event::read()? {
                if key.kind == KeyEventKind::Press {
                    match key.code {
                        KeyCode::Char(c) => {
                            self.input.push(c);
                            self.history_scroll = 0;
                        },
                        KeyCode::Backspace => { self.input.pop(); },
                        KeyCode::Enter => {
                            if self.input.is_empty() == false {
                                let args = self.input.split(" ").map(|s| s.to_string()).collect::<Vec<String>>();
                                if let Err(e) = cmd_tx.send(args.clone()).await {
                                    error!("Error occured sending a command to the server! {e}");
                                    return Ok(true);
                                }
                                self.input_history.insert(0, self.input.clone());
                                self.input = String::new();
                                if args.get(0) == Some(&"exit".to_string()) {
                                    return Ok(true);
                                }
                            }
                        },
                        KeyCode::Up => {
                            if self.history_scroll < self.input_history.len() {
                                self.history_scroll += 1;
                                self.input = self.input_history[self.history_scroll - 1].clone();
                            }
                        },
                        KeyCode::Down => {
                            if self.history_scroll > 1 {
                                self.history_scroll -= 1;
                                self.input = self.input_history[self.history_scroll - 1].clone();
                            }
                        },
                        _ => {},
                    }
                }
            }
        }

        Ok(false)
    }

    fn draw(&mut self) -> Result<()> {
        self.terminal.draw(|frame| {
            let area = frame.size();

            let vert_layout = Layout::default()
                .direction(Direction::Vertical)
                .constraints(vec![
                    Constraint::Length(area.height - 3),
                    Constraint::Length(3),
                ])
                .split(area);

            let horiz_layout = Layout::default()
                .direction(Direction::Horizontal)
                .constraints(vec![
                    Constraint::Percentage(70),
                    Constraint::Percentage(30),
                ])
                .split(vert_layout[0]);

            let mut lines = Vec::new();
            for (i, (level, msg)) in self.log_buffer.iter().enumerate() {
                if i >= (area.height as usize - 5) { break; }
                let (level_style, spacer) = match level {
                    Level::Info => (Style::default().green(), " "),
                    Level::Warn => (Style::default().yellow(), " "),
                    Level::Error => (Style::default().red(), ""),
                    Level::Debug => (Style::default().gray(), ""),
                    Level::Trace => (Style::default().cyan(), ""),
                };
                lines.push(Line::from(vec![
                    Span::styled("[", Style::default()),
                    Span::styled(format!("{}", level), level_style),
                    Span::styled(format!("]{} {}", spacer, msg), Style::default()),
                ]));
            }
            lines.reverse();
            frame.render_widget(
                // Paragraph::new("[INFO] info!!!\n[DEBUG] debug!!!!").block(Block::new().borders(Borders::ALL)),
                Paragraph::new(Text::from(lines)).block(Block::new().borders(Borders::ALL).title("Log")),
                horiz_layout[0],
            );

            frame.render_widget(
                Paragraph::new("0 - luuk-bepis\n1 - lion guy\n2 - the_racc").block(Block::new().borders(Borders::ALL).title("Player List")),
                horiz_layout[1],
            );

            frame.render_widget(
                Paragraph::new(format!(" > {}", self.input)).block(Block::new().borders(Borders::ALL).title("Input")),
                vert_layout[1],
            );
        })?;

        Ok(())
    }
}
